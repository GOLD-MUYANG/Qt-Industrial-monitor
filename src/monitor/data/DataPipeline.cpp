#include "DataPipeline.h"

#include <cmath>

using namespace industrial::protocol;

DataPipeline::DataPipeline(QObject *parent)
    : QObject(parent)
{
}

void DataPipeline::processSamples(const SampleBatch &samples)
{
    if (samples.isEmpty()) {
        emitDataError({}, 0, QStringLiteral("样本批次不能为空"));
        return;
    }

    const auto &first = samples.constFirst();
    if (first.deviceId.trimmed().isEmpty() || first.sequence == 0
        || !first.timestampUtc.isValid() || first.timestampUtc.timeSpec() != Qt::UTC) {
        emitDataError(first.deviceId, first.sequence,
                      QStringLiteral("设备 ID、序号或 UTC 时间戳无效"));
        return;
    }

    auto &device = m_devices[first.deviceId];
    if (first.sequence <= device.lastSequence) {
        emitDataError(first.deviceId, first.sequence,
                      QStringLiteral("样本序号必须严格递增"));
        return;
    }
    if (device.lastTimestampUtc.isValid()
        && first.timestampUtc < device.lastTimestampUtc) {
        emitDataError(first.deviceId, first.sequence,
                      QStringLiteral("样本时间戳不能早于上一批次"));
        return;
    }

    for (const auto &sample : samples) {
        if (sample.deviceId != first.deviceId || sample.sequence != first.sequence
            || sample.timestampUtc != first.timestampUtc) {
            emitDataError(first.deviceId, first.sequence,
                          QStringLiteral("同一批次的设备、序号和时间戳必须一致"));
            return;
        }
    }

    device.lastSequence = first.sequence;
    device.lastTimestampUtc = first.timestampUtc;

    RealtimeSnapshotBatch output;
    output.reserve(samples.size());
    for (const auto &sample : samples) {
        if (!isKnownTag(sample.tagId)) {
            emitDataError(sample.deviceId, sample.sequence,
                          QStringLiteral("未知测点：%1").arg(sample.tagId));
            continue;
        }

        auto &tag = device.tags[sample.tagId];
        if (sample.quality != DataQuality::Good) {
            if (!tag.hasSnapshot) {
                emitDataError(sample.deviceId, sample.sequence,
                              QStringLiteral("非 Good 样本没有可保留的历史值"));
                continue;
            }
            auto snapshot = tag.lastSnapshot;
            snapshot.quality = sample.quality;
            snapshot.timestampUtc = sample.timestampUtc;
            snapshot.sequence = sample.sequence;
            tag.lastSnapshot = snapshot;
            output.append(snapshot);
            continue;
        }

        if (!std::isfinite(sample.engineeringValue) || !isInRange(sample)) {
            emitDataError(sample.deviceId, sample.sequence,
                          QStringLiteral("测点 %1 的工程值超出有效范围")
                              .arg(sample.tagId));
            if (tag.hasSnapshot) {
                auto snapshot = tag.lastSnapshot;
                snapshot.quality = DataQuality::Bad;
                snapshot.timestampUtc = sample.timestampUtc;
                snapshot.sequence = sample.sequence;
                tag.lastSnapshot = snapshot;
                output.append(snapshot);
            }
            continue;
        }

        tag.statistics.add(sample.engineeringValue, sample.timestampUtc);
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
        tag.hasSnapshot = true;
        output.append(tag.lastSnapshot);
    }

    if (!output.isEmpty()) {
        emit snapshotsReady(output);
    }
}

void DataPipeline::handleDeviceState(const DeviceState &state)
{
    auto deviceIt = m_devices.find(state.deviceId);
    if (deviceIt == m_devices.end()) {
        return;
    }

    const ConnectionState previous = deviceIt->connectionState;
    deviceIt->connectionState = state.connectionState;
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
