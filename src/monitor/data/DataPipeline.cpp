#include "DataPipeline.h"

#include <cmath>

using namespace industrial::protocol;

DataPipeline::DataPipeline(QObject *parent)
    : QObject(parent)
    , m_alarmEngine(this)
{
    connect(&m_alarmEngine, &industrial::monitor::AlarmEngine::alarmChanged,
            this, &DataPipeline::alarmChanged);
    connect(&m_alarmEngine, &industrial::monitor::AlarmEngine::ruleRejected,
            this, [this](const QString &ruleId, const QString &reason) {
                emitDataError({}, 0,
                              QStringLiteral("报警规则 %1 无效：%2")
                                  .arg(ruleId, reason));
            });
}

void DataPipeline::processSamples(const SampleBatch &samples)
{
    /*
     * 处理一次设备轮询产生的整批样本。
     *
     * 这个函数运行在 data-pipeline 线程，是通信层与业务层之间的数据门禁：
     * 1. 先检查整批数据的设备、序号和时间关系；
     * 2. 再逐个检查测点、质量和工程值；
     * 3. 有效 Good 样本同时进入统计、报警和存储；
     * 4. Good 以及可展示的 Stale/Bad 状态最终生成实时快照交给界面。
     *
     * 原始 samples 不会直接进入数据库，只有本函数筛选后的
     * validatedSamplesReady 才是测量值存储入口。
     */

    // 空批次既没有设备身份，也没有可处理的测点，直接报告数据错误。
    if (samples.isEmpty()) {
        emitDataError({}, 0, QStringLiteral("样本批次不能为空"));
        return;
    }

    /*
     * 同一次 Modbus 读取中的所有测点应属于同一设备、同一序号和同一采集时刻，
     * 因此先用首个样本建立本批次的公共基准。
     */
    const auto &first = samples.constFirst();

    // 批次基准必须包含设备 ID、非零序号和明确的 UTC 时间戳。
    if (first.deviceId.trimmed().isEmpty() || first.sequence == 0
        || !first.timestampUtc.isValid() || first.timestampUtc.timeSpec() != Qt::UTC) {
        emitDataError(first.deviceId, first.sequence,
                      QStringLiteral("设备 ID、序号或 UTC 时间戳无效"));
        return;
    }

    // 取得该设备在数据线程中的运行时状态；首次出现时会创建一份空状态。
    auto &device = m_devices[first.deviceId];

    /*
     * 序号必须严格递增，用来拒绝重复批次和乱序到达的旧批次。
     * 正常的自动重连不会重置该基线；有序重建设备会话时才会单独重置。
     */
    if (first.sequence <= device.lastSequence) {
        emitDataError(first.deviceId, first.sequence,
                      QStringLiteral("样本序号必须严格递增"));
        return;
    }

    // 即使序号递增，采集时间也不能倒退，否则历史和滚动统计的时间顺序会被破坏。
    if (device.lastTimestampUtc.isValid()
        && first.timestampUtc < device.lastTimestampUtc) {
        emitDataError(first.deviceId, first.sequence,
                      QStringLiteral("样本时间戳不能早于上一批次"));
        return;
    }

    /*
     * 检查批次内部一致性。任意一个样本不属于首样本所代表的设备、序号或时刻，
     * 都说明通信层拼出了混合批次，因此整批拒绝，避免只保存半批数据。
     */
    for (const auto &sample : samples) {
        if (sample.deviceId != first.deviceId || sample.sequence != first.sequence
            || sample.timestampUtc != first.timestampUtc) {
            emitDataError(first.deviceId, first.sequence,
                          QStringLiteral("同一批次的设备、序号和时间戳必须一致"));
            return;
        }
    }

    /*
     * 到这里，批次级门禁已经通过。立即推进该设备的时序基线。
     * 后续某个测点即使因未知、非 Good 或越界被跳过，这个批次序号也已经消费，
     * 从而避免同一坏批次被重复提交。
     */
    device.lastSequence = first.sequence;
    device.lastTimestampUtc = first.timestampUtc;

    /*
     * output：交给界面的实时快照，可能包含 Good，也可能包含沿用旧值的 Stale/Bad。
     * alarmSamples：通过全部业务校验的 Good 原始样本。名称虽然强调报警，
     *                但它同时也是后面 validatedSamplesReady 的存储数据源。
     */
    RealtimeSnapshotBatch output;
    SampleBatch alarmSamples;

    // 最多为每个输入样本生成一个快照，预留容量可减少容器扩容。
    output.reserve(samples.size());

    // 批次级校验完成后，开始逐个处理测点。
    for (const auto &sample : samples) {
        // 未配置在系统中的测点无法确定工程范围和展示含义，只跳过该点，不丢弃其他点。
        if (!isKnownTag(sample.tagId)) {
            emitDataError(sample.deviceId, sample.sequence,
                          QStringLiteral("未知测点：%1").arg(sample.tagId));
            continue;
        }

        // 取得该测点的滚动统计与最近快照；首次出现时创建空状态。
        auto &tag = device.tags[sample.tagId];

        /*
         * Stale/Bad 不代表新的可信测量值，因此不进入统计、报警或数据库。
         * 如果以前存在 Good 快照，就保留它的数值和统计结果，只把质量、时间、序号
         * 更新为本次状态，让界面表达“当前数据不可用，但最后值仍可参考”。
         */
        if (sample.quality != DataQuality::Good) {
            // 没有历史 Good 值时不存在可以沿用的数值，因此连展示快照也不能生成。
            if (!tag.hasSnapshot) {
                emitDataError(sample.deviceId, sample.sequence,
                              QStringLiteral("非 Good 样本没有可保留的历史值"));
                continue;
            }

            // 复制最近一次有效快照，数值与统计字段保持不变。
            auto snapshot = tag.lastSnapshot;

            // 用本次样本更新“当前质量”和状态发生时间。
            snapshot.quality = sample.quality;
            snapshot.timestampUtc = sample.timestampUtc;
            snapshot.sequence = sample.sequence;

            // 保存新的展示状态，并加入本次 UI 输出。
            tag.lastSnapshot = snapshot;
            output.append(snapshot);
            continue;
        }

        /*
         * 即使通信层把质量标记为 Good，业务层仍要拒绝 NaN、无穷大和工程越界值。
         * 这些值不会污染统计窗口，也不会触发报警或写入测量历史。
         */
        if (!std::isfinite(sample.engineeringValue) || !isInRange(sample)) {
            emitDataError(sample.deviceId, sample.sequence,
                          QStringLiteral("测点 %1 的工程值超出有效范围")
                              .arg(sample.tagId));
            continue;
        }

        // 只有通过全部校验的 Good 工程值才能进入最近 60 秒滚动统计。
        tag.statistics.add(sample.engineeringValue, sample.timestampUtc);

        /*
         * 用最新统计结果构造实时快照。快照面向展示，除了当前值，还携带
         * 最小值、最大值、平均值、样本数、质量、时间和批次序号。
         */
        tag.lastSnapshot = {
            sample.deviceId,
            sample.tagId,
            tag.statistics.current(),
            tag.statistics.minimum(),
            tag.statistics.maximum(),
            tag.statistics.average(),
            tag.statistics.count(),
            DataQuality::Good,
            sample.timestampUtc,
            sample.sequence
        };

        // 标记该测点已经拥有可供后续 Stale/Bad 沿用的可信快照。
        tag.hasSnapshot = true;

        // Good 快照进入本次界面输出。
        output.append(tag.lastSnapshot);

        // Good 原始样本进入报警/存储候选集合，二者使用同一份已验证数据。
        alarmSamples.append(sample);
    }

    /*
     * 先让报警引擎消费有效 Good 样本：它会根据规则累计连续超限/恢复次数，
     * 必要时通过 alarmChanged 发出激活或恢复记录。
     * 随后发出 validatedSamplesReady，通过 queued connection 交给 SQLite 写线程。
     */
    if (!alarmSamples.isEmpty()) {
        m_alarmEngine.processSamples(alarmSamples);
        emit validatedSamplesReady(alarmSamples);
    }

    /*
     * UI 快照与存储样本的边界不同：
     * - 存储只接收上面的有效 Good 样本；
     * - UI 既接收 Good 快照，也接收沿用最后有效值生成的 Stale/Bad 快照。
     */
    if (!output.isEmpty()) {
        emit snapshotsReady(output);
    }
}

void DataPipeline::handleDeviceState(const DeviceState &state)
{
    m_alarmEngine.handleDeviceState(state);
    auto deviceIt = m_devices.find(state.deviceId);
    if (deviceIt == m_devices.end()) {
        return;
    }

    const ConnectionState previous = deviceIt->connectionState;
    deviceIt->connectionState = state.connectionState;
    if (state.connectionState == ConnectionState::Stopped
        && previous == ConnectionState::Stopping) {
        // 重新创建设备 Worker 后序号会从 1 开始；仅在有序停止边界重置，
        // 自动重连仍保留旧序号以继续拒绝乱序数据。
        deviceIt->lastSequence = 0;
        deviceIt->lastTimestampUtc = {};
    }
    const bool shouldMarkStale = state.connectionState == ConnectionState::Reconnecting
        || (state.connectionState == ConnectionState::Stopped
            && previous != ConnectionState::Stopping);
    if (!shouldMarkStale) {
        return;
    }

    RealtimeSnapshotBatch output;
    const QDateTime timestampUtc = QDateTime::currentDateTimeUtc();
    for (auto tagIt = deviceIt->tags.begin(); tagIt != deviceIt->tags.end(); ++tagIt) {
        if (!tagIt->hasSnapshot) {
            continue;
        }
        tagIt->lastSnapshot.quality = DataQuality::Stale;
        tagIt->lastSnapshot.timestampUtc = timestampUtc;
        output.append(tagIt->lastSnapshot);
    }
    if (!output.isEmpty()) {
        emit snapshotsReady(output);
    }
}

void DataPipeline::resetDeviceSession(const QString &deviceId)
{
    auto deviceIt = m_devices.find(deviceId);
    if (deviceIt == m_devices.end()) {
        return;
    }
    deviceIt->lastSequence = 0;
    deviceIt->lastTimestampUtc = {};
}

void DataPipeline::setAlarmRules(
    const industrial::monitor::AlarmRuleList &rules)
{
    m_alarmEngine.setRules(rules);
}

void DataPipeline::acknowledgeAlarm(const QString &alarmId,
                                    const QString &note)
{
    if (!m_alarmEngine.acknowledge(alarmId, note)) {
        emitDataError({}, 0, QStringLiteral("报警不存在或已经确认"));
    }
}

bool DataPipeline::isKnownTag(const QString &tagId)
{
    return tagId == QStringLiteral("temperature")
        || tagId == QStringLiteral("pressure")
        || tagId == QStringLiteral("speed")
        || tagId == QStringLiteral("voltage")
        || tagId == QStringLiteral("status");
}

bool DataPipeline::isInRange(const MeasurementSample &sample)
{
    const double value = sample.engineeringValue;
    if (sample.tagId == QStringLiteral("temperature")) {
        return value >= -50.0 && value <= 200.0;
    }
    if (sample.tagId == QStringLiteral("pressure")) {
        return value >= 0.0 && value <= 10.0;
    }
    if (sample.tagId == QStringLiteral("speed")) {
        return value >= 0.0 && value <= 6'000.0;
    }
    if (sample.tagId == QStringLiteral("voltage")) {
        return value >= 0.0 && value <= 500.0;
    }
    return value >= 0.0 && value <= 15.0;
}

void DataPipeline::emitDataError(const QString &deviceId,
                                 quint64 sequence,
                                 const QString &message)
{
    DeviceError error;
    error.deviceId = deviceId;
    error.code = -1;
    error.message = message;
    error.category = DeviceErrorCategory::Data;
    error.requestId = sequence;
    error.recoverable = true;
    emit pipelineError(error);
}
