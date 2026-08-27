#pragma once

#include "AlarmTypes.h"
#include "PluginManager.h"
#include "VisionTypes.h"

#include <QPointer>
#include <QThread>

#include <industrial/protocol/ProtocolTypes.h>

#include <memory>

class DataPipeline;
class DeviceSession;
class HistoryWorker;
class StorageWorker;
namespace industrial::monitor::vision {
class VisionSession;
}

class ApplicationController final : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationController(const QString &pluginDirectory,
                                   const QString &databasePath,
                                   QObject *parent = nullptr);
    ~ApplicationController() override;

    bool start();
    bool shutdown(int timeoutMs = 3'000);
    bool isRunning() const;

public slots:
    void connectDevice();
    bool disconnectDevice();
    void writeTargetSpeed(quint16 targetSpeed);
    void applyDeviceConfig(
        const industrial::protocol::DeviceConfig &config);
    void acknowledgeAlarm(const QString &alarmId, const QString &note);
    void openVisionVideo(const QString &path);
    void playVisionVideo();
    void pauseVisionVideo();
    void stopVisionVideo();

signals:
    void protocolAvailable(
        const industrial::protocol::ProtocolDescriptor &descriptor);
    void initialized(const industrial::protocol::DeviceConfig &device);
    void snapshotsReady(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);
    void alarmChanged(const industrial::monitor::AlarmRecord &alarm);
    void deviceConfigChanged(
        const industrial::protocol::DeviceConfig &device);
    void deviceStateChanged(
        const industrial::protocol::DeviceState &state);
    void writeFinished(
        const industrial::protocol::WriteResult &result);
    void storageStatusChanged(const QString &message, bool healthy);
    void fatalError(const QString &message);
    void visionSourceOpened(
        const industrial::monitor::vision::VisionSourceInfo &source);
    void visionFrameReady(
        const industrial::monitor::vision::VisionFrameResult &frame);
    void visionStateChanged(
        industrial::monitor::vision::VisionPlaybackState state,
        const QString &message);
    void visionError(const QString &message);

    // 内部命令信号只负责跨线程投递，调用方看不到 Worker 指针。
    void configureAlarmRulesRequested(
        const industrial::monitor::AlarmRuleList &rules);
    void acknowledgeAlarmRequested(const QString &alarmId,
                                   const QString &note);
    void startStorageRequested(const QString &databasePath);
    void saveDeviceRequested(
        const industrial::protocol::DeviceConfig &device);
    void startHistoryRequested(const QString &databasePath);

private:
    bool stopDeviceSession(int timeoutMs);
    void setupDataThread();
    void setupStorageThread();
    void setupHistoryThread();
    void setupVisionSession();
    void reportCommunicationError(
        const industrial::protocol::DeviceError &error);

    QString m_pluginDirectory;
    QString m_databasePath;

    // PluginManager 声明在 DeviceSession 之前，逆序析构可保证插件最后释放。
    PluginManager m_pluginManager;
    industrial::protocol::IProtocolPlugin *m_plugin = nullptr;
    std::unique_ptr<DeviceSession> m_session;
    std::unique_ptr<industrial::monitor::vision::VisionSession> m_visionSession;

    QThread m_dataThread;
    QThread m_storageThread;
    QThread m_historyThread;
    QPointer<DataPipeline> m_pipeline;
    QPointer<StorageWorker> m_storage;
    QPointer<HistoryWorker> m_history;

    industrial::protocol::DeviceConfig m_device;
    bool m_started = false;
    bool m_initialized = false;
    bool m_restartAfterSave = false;
    quint64 m_nextWriteRequestId = 1;
};
