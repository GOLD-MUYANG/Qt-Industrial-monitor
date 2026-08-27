#include "QmlApplicationFacade.h"

#include "ApplicationController.h"

#include <QDateTime>

using namespace industrial::monitor;
using namespace industrial::protocol;

QmlApplicationFacade::QmlApplicationFacade(ApplicationController *controller,
                                           QObject *parent)
    : QObject(parent), m_controller(controller)
{
    Q_ASSERT(m_controller);

    connect(m_controller, &ApplicationController::protocolAvailable, this,
            &QmlApplicationFacade::handleProtocolAvailable);
    connect(m_controller, &ApplicationController::initialized, this,
            &QmlApplicationFacade::handleInitialized);
    connect(m_controller, &ApplicationController::snapshotsReady, this,
            &QmlApplicationFacade::handleSnapshots);
    connect(m_controller, &ApplicationController::alarmChanged, this,
            &QmlApplicationFacade::handleAlarmChanged);
    connect(m_controller, &ApplicationController::deviceConfigChanged, this,
            &QmlApplicationFacade::handleDeviceConfigChanged);
    connect(m_controller, &ApplicationController::deviceStateChanged, this,
            &QmlApplicationFacade::handleDeviceStateChanged);
    connect(m_controller, &ApplicationController::writeFinished, this,
            &QmlApplicationFacade::handleWriteFinished);
    connect(m_controller, &ApplicationController::storageStatusChanged, this,
            &QmlApplicationFacade::handleStorageStatus);
    connect(m_controller, &ApplicationController::fatalError, this,
            &QmlApplicationFacade::handleFatalError);

    connect(this, &QmlApplicationFacade::connectDeviceRequested,
            m_controller, &ApplicationController::connectDevice);
    connect(this, &QmlApplicationFacade::writeTargetSpeedRequested,
            m_controller, &ApplicationController::writeTargetSpeed);
    connect(this, &QmlApplicationFacade::saveDeviceRequested,
            m_controller, &ApplicationController::applyDeviceConfig);
    connect(this, &QmlApplicationFacade::acknowledgeAlarmRequested,
            m_controller, &ApplicationController::acknowledgeAlarm);
}

QmlRealtimeModel *QmlApplicationFacade::realtimeModel() { return &m_realtimeModel; }
QmlTrendModel *QmlApplicationFacade::trendModel() { return &m_trendModel; }
QmlDeviceViewModel *QmlApplicationFacade::deviceModel() { return &m_deviceModel; }
QmlProtocolModel *QmlApplicationFacade::protocolModel() { return &m_protocolModel; }
QmlAlarmModel *QmlApplicationFacade::activeAlarmModel() { return &m_activeAlarmModel; }
QmlAlarmModel *QmlApplicationFacade::alarmHistoryModel() { return &m_alarmHistoryModel; }
QString QmlApplicationFacade::statusMessage() const { return m_statusMessage; }
bool QmlApplicationFacade::statusHealthy() const { return m_statusHealthy; }
bool QmlApplicationFacade::initialized() const { return m_initialized; }
bool QmlApplicationFacade::commandBusy() const { return m_commandBusy; }
bool QmlApplicationFacade::displayPaused() const { return m_trendModel.displayPaused(); }

void QmlApplicationFacade::connectDevice()
{
    if (!ensureReady() || !ensureIdle())
    {
        return;
    }
    if (!m_device.enabled)
    {
        setStatus(QStringLiteral("设备已停用，无法连接"), false);
        return;
    }
    if (static_cast<ConnectionState>(m_deviceModel.connectionState())
        != ConnectionState::Stopped)
    {
        setStatus(QStringLiteral("当前连接状态不允许重复连接"), false);
        return;
    }
    beginCommand(PendingCommand::Connect);
    setStatus(QStringLiteral("正在连接设备…"), true);
    emit connectDeviceRequested();
}

void QmlApplicationFacade::disconnectDevice()
{
    if (!ensureReady() || !ensureIdle())
    {
        return;
    }
    beginCommand(PendingCommand::Disconnect);
    setStatus(QStringLiteral("正在断开设备…"), true);
    const bool stopped = m_controller->disconnectDevice();
    finishCommand(PendingCommand::Disconnect);
    setStatus(stopped ? QStringLiteral("设备已断开")
                      : QStringLiteral("设备断开失败"),
              stopped);
}

void QmlApplicationFacade::writeTargetSpeed(int targetSpeed)
{
    if (!ensureReady() || !ensureIdle())
    {
        return;
    }
    if (targetSpeed < 0 || targetSpeed > 65'535)
    {
        setStatus(QStringLiteral("目标转速必须在 0 到 65535 rpm 之间"), false);
        return;
    }

    beginCommand(PendingCommand::Write);
    setStatus(QStringLiteral("正在写入目标转速…"), true);
    emit writeTargetSpeedRequested(static_cast<quint16>(targetSpeed));
}

void QmlApplicationFacade::saveDevice(const QString &protocolKey,
                                      const QString &host,
                                      int port,
                                      int unitId,
                                      int pollIntervalMs,
                                      int timeoutMs,
                                      bool enabled)
{
    if (!ensureReady() || !ensureIdle())
    {
        return;
    }
    if (m_protocolModel.indexOfKey(protocolKey) < 0)
    {
        setStatus(QStringLiteral("请选择已加载的通信协议"), false);
        return;
    }
    if (host.trimmed().isEmpty())
    {
        setStatus(QStringLiteral("主机地址不能为空"), false);
        return;
    }
    if (port < 1 || port > 65'535)
    {
        setStatus(QStringLiteral("端口必须在 1 到 65535 之间"), false);
        return;
    }
    if (unitId < 1 || unitId > 247)
    {
        setStatus(QStringLiteral("Unit ID 必须在 1 到 247 之间"), false);
        return;
    }
    if (pollIntervalMs < 50)
    {
        setStatus(QStringLiteral("轮询间隔不能小于 50 ms"), false);
        return;
    }
    if (timeoutMs < 1)
    {
        setStatus(QStringLiteral("超时时间必须大于 0 ms"), false);
        return;
    }

    DeviceConfig updated = m_device;
    updated.protocolKey = protocolKey;
    updated.host = host.trimmed();
    updated.port = static_cast<quint16>(port);
    updated.unitId = unitId;
    updated.pollIntervalMs = pollIntervalMs;
    updated.timeoutMs = timeoutMs;
    updated.enabled = enabled;
    beginCommand(PendingCommand::Save);
    setStatus(QStringLiteral("正在保存设备配置…"), true);
    emit saveDeviceRequested(updated);
}

void QmlApplicationFacade::acknowledgeAlarm(const QString &alarmId,
                                            const QString &note)
{
    if (!ensureReady() || !ensureIdle())
    {
        return;
    }
    if (alarmId.trimmed().isEmpty())
    {
        setStatus(QStringLiteral("请选择需要确认的报警"), false);
        return;
    }
    if (!m_alarmHistoryModel.isAcknowledgeableId(alarmId))
    {
        setStatus(QStringLiteral("所选报警不存在或当前不可确认"), false);
        return;
    }
    m_pendingAlarmId = alarmId;
    beginCommand(PendingCommand::Acknowledge);
    setStatus(QStringLiteral("正在确认报警…"), true);
    emit acknowledgeAlarmRequested(alarmId, note.trimmed());
}

void QmlApplicationFacade::selectTrendTag(const QString &tagId)
{
    m_trendModel.selectTag(tagId);
}

void QmlApplicationFacade::setDisplayPaused(bool paused)
{
    if (m_trendModel.displayPaused() == paused)
    {
        return;
    }
    m_trendModel.setDisplayPaused(paused);
    emit displayPausedChanged();
}

void QmlApplicationFacade::handleProtocolAvailable(
    const ProtocolDescriptor &descriptor)
{
    m_protocolModel.addProtocol(descriptor);
    m_deviceModel.setProtocolDisplayName(descriptor.key,
                                         descriptor.displayName);
}

void QmlApplicationFacade::handleInitialized(const DeviceConfig &device)
{
    m_device = device;
    m_deviceModel.setDevice(device);
    if (!m_initialized)
    {
        m_initialized = true;
        emit initializedChanged();
    }
    finishAnyCommand();
    setStatus(QStringLiteral("系统初始化完成"), true);
}

void QmlApplicationFacade::handleSnapshots(
    const RealtimeSnapshotBatch &snapshots)
{
    m_realtimeModel.applySnapshots(snapshots);
    m_trendModel.applySnapshots(snapshots);

    QDateTime latestGoodTimestamp;
    for (const auto &snapshot : snapshots)
    {
        if (snapshot.quality == DataQuality::Good
            && snapshot.timestampUtc.isValid()
            && (!latestGoodTimestamp.isValid()
                || snapshot.timestampUtc > latestGoodTimestamp))
        {
            latestGoodTimestamp = snapshot.timestampUtc;
        }
    }
    if (latestGoodTimestamp.isValid())
    {
        m_deviceModel.setLastCommunicationTime(latestGoodTimestamp);
    }
}

void QmlApplicationFacade::handleAlarmChanged(const AlarmRecord &alarm)
{
    m_activeAlarmModel.upsertAlarm(alarm);
    m_alarmHistoryModel.upsertAlarm(alarm);
    const bool acknowledgementFinished =
        alarm.state == AlarmState::ActiveAcknowledged
        || alarm.state == AlarmState::RecoveredAcknowledged;
    if (m_pendingCommand == PendingCommand::Acknowledge
        && alarm.id == m_pendingAlarmId
        && acknowledgementFinished)
    {
        finishCommand(PendingCommand::Acknowledge);
        setStatus(QStringLiteral("报警状态已更新"), true);
    }
}

void QmlApplicationFacade::handleDeviceConfigChanged(const DeviceConfig &device)
{
    m_device = device;
    m_deviceModel.setDevice(device);
    if (finishCommand(PendingCommand::Save))
    {
        setStatus(QStringLiteral("设备配置已保存"), true);
    }
}

void QmlApplicationFacade::handleDeviceStateChanged(const DeviceState &state)
{
    m_deviceModel.setState(state);
    const bool transitionFinished = state.connectionState == ConnectionState::Online
        || state.connectionState == ConnectionState::Reconnecting
        || state.connectionState == ConnectionState::Stopped
        || state.connectionState == ConnectionState::Faulted;
    if (transitionFinished)
    {
        finishCommand(PendingCommand::Connect);
    }
    setStatus(state.message.isEmpty() ? m_deviceModel.connectionStateText()
                                      : state.message,
              state.connectionState != ConnectionState::Faulted);
}

void QmlApplicationFacade::handleWriteFinished(const WriteResult &result)
{
    finishCommand(PendingCommand::Write);
    setStatus(result.success
                  ? QStringLiteral("目标转速写入成功")
                  : QStringLiteral("目标转速写入失败：%1")
                        .arg(result.errorMessage),
              result.success);
}

void QmlApplicationFacade::handleStorageStatus(const QString &message,
                                               bool healthy)
{
    if (!healthy && m_pendingCommand == PendingCommand::Save)
    {
        finishCommand(PendingCommand::Save);
    }
    setStatus(message, healthy);
}

void QmlApplicationFacade::handleFatalError(const QString &message)
{
    finishAnyCommand();
    setStatus(message, false);
}

bool QmlApplicationFacade::ensureReady()
{
    if (m_initialized)
    {
        return true;
    }
    setStatus(QStringLiteral("系统尚未初始化完成"), false);
    return false;
}

bool QmlApplicationFacade::ensureIdle()
{
    if (!m_commandBusy)
    {
        return true;
    }
    setStatus(QStringLiteral("上一条命令仍在处理中"), false);
    return false;
}

void QmlApplicationFacade::setStatus(const QString &message, bool healthy)
{
    if (m_statusMessage != message)
    {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
    if (m_statusHealthy != healthy)
    {
        m_statusHealthy = healthy;
        emit statusHealthyChanged();
    }
}

void QmlApplicationFacade::beginCommand(PendingCommand command)
{
    m_pendingCommand = command;
    if (m_commandBusy)
    {
        return;
    }
    m_commandBusy = true;
    emit commandBusyChanged();
}

bool QmlApplicationFacade::finishCommand(PendingCommand command)
{
    if (m_pendingCommand != command)
    {
        return false;
    }
    m_pendingCommand = PendingCommand::None;
    m_pendingAlarmId.clear();
    if (m_commandBusy)
    {
        m_commandBusy = false;
        emit commandBusyChanged();
    }
    return true;
}

void QmlApplicationFacade::finishAnyCommand()
{
    m_pendingCommand = PendingCommand::None;
    m_pendingAlarmId.clear();
    if (!m_commandBusy)
    {
        return;
    }
    m_commandBusy = false;
    emit commandBusyChanged();
}
