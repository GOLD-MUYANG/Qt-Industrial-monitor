#pragma once

#include "QmlAlarmModel.h"
#include "QmlDeviceViewModel.h"
#include "QmlProtocolModel.h"
#include "QmlRealtimeModel.h"
#include "QmlTrendModel.h"

#include <QObject>

class ApplicationController;

class QmlApplicationFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QmlRealtimeModel *realtimeModel READ realtimeModel CONSTANT)
    Q_PROPERTY(QmlTrendModel *trendModel READ trendModel CONSTANT)
    Q_PROPERTY(QmlDeviceViewModel *deviceModel READ deviceModel CONSTANT)
    Q_PROPERTY(QmlProtocolModel *protocolModel READ protocolModel CONSTANT)
    Q_PROPERTY(QmlAlarmModel *activeAlarmModel READ activeAlarmModel CONSTANT)
    Q_PROPERTY(QmlAlarmModel *alarmHistoryModel READ alarmHistoryModel CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool statusHealthy READ statusHealthy NOTIFY statusHealthyChanged)
    Q_PROPERTY(bool initialized READ initialized NOTIFY initializedChanged)
    Q_PROPERTY(bool commandBusy READ commandBusy NOTIFY commandBusyChanged)
    Q_PROPERTY(bool displayPaused READ displayPaused NOTIFY displayPausedChanged)

public:
    explicit QmlApplicationFacade(ApplicationController *controller,
                                  QObject *parent = nullptr);

    QmlRealtimeModel *realtimeModel();
    QmlTrendModel *trendModel();
    QmlDeviceViewModel *deviceModel();
    QmlProtocolModel *protocolModel();
    QmlAlarmModel *activeAlarmModel();
    QmlAlarmModel *alarmHistoryModel();

    QString statusMessage() const;
    bool statusHealthy() const;
    bool initialized() const;
    bool commandBusy() const;
    bool displayPaused() const;

    Q_INVOKABLE void connectDevice();
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void writeTargetSpeed(int targetSpeed);
    Q_INVOKABLE void saveDevice(const QString &protocolKey,
                                const QString &host,
                                int port,
                                int unitId,
                                int pollIntervalMs,
                                int timeoutMs,
                                bool enabled);
    Q_INVOKABLE void acknowledgeAlarm(const QString &alarmId,
                                      const QString &note);
    Q_INVOKABLE void selectTrendTag(const QString &tagId);
    Q_INVOKABLE void setDisplayPaused(bool paused);

signals:
    // 这些强类型信号只连接现有 Controller，QML 侧始终只调用基础类型方法。
    void connectDeviceRequested();
    void writeTargetSpeedRequested(quint16 targetSpeed);
    void saveDeviceRequested(
        const industrial::protocol::DeviceConfig &device);
    void acknowledgeAlarmRequested(const QString &alarmId,
                                   const QString &note);

    void statusMessageChanged();
    void statusHealthyChanged();
    void initializedChanged();
    void commandBusyChanged();
    void displayPausedChanged();

private slots:
    void handleProtocolAvailable(
        const industrial::protocol::ProtocolDescriptor &descriptor);
    void handleInitialized(
        const industrial::protocol::DeviceConfig &device);
    void handleSnapshots(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);
    void handleAlarmChanged(
        const industrial::monitor::AlarmRecord &alarm);
    void handleDeviceConfigChanged(
        const industrial::protocol::DeviceConfig &device);
    void handleDeviceStateChanged(
        const industrial::protocol::DeviceState &state);
    void handleWriteFinished(
        const industrial::protocol::WriteResult &result);
    void handleStorageStatus(const QString &message, bool healthy);
    void handleFatalError(const QString &message);

private:
    enum class PendingCommand
    {
        None,
        Connect,
        Disconnect,
        Write,
        Save,
        Acknowledge
    };

    bool ensureReady();
    bool ensureIdle();
    void setStatus(const QString &message, bool healthy);
    void beginCommand(PendingCommand command);
    bool finishCommand(PendingCommand command);
    void finishAnyCommand();

    ApplicationController *m_controller = nullptr;
    QmlRealtimeModel m_realtimeModel;
    QmlTrendModel m_trendModel;
    QmlDeviceViewModel m_deviceModel;
    QmlProtocolModel m_protocolModel;
    QmlAlarmModel m_activeAlarmModel{QmlAlarmModel::Mode::Active};
    QmlAlarmModel m_alarmHistoryModel{QmlAlarmModel::Mode::History};
    industrial::protocol::DeviceConfig m_device;
    QString m_statusMessage = QStringLiteral("正在初始化系统…");
    bool m_statusHealthy = true;
    bool m_initialized = false;
    bool m_commandBusy = false;
    PendingCommand m_pendingCommand = PendingCommand::None;
    QString m_pendingAlarmId;
};
