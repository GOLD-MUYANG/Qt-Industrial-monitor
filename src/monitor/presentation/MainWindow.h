#pragma once

#include <QMainWindow>

#include "AlarmTypes.h"
#include "VisionTypes.h"

#include <industrial/protocol/ProtocolTypes.h>

class AlarmPage;
class DevicePage;
class RealtimePage;
class VisionPage;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void addProtocol(
        const industrial::protocol::ProtocolDescriptor &descriptor);
    void applySnapshots(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);
    void setDevice(const industrial::protocol::DeviceConfig &config);
    void setDeviceState(const industrial::protocol::DeviceState &state);
    void upsertAlarm(const industrial::monitor::AlarmRecord &alarm);
    void setStorageStatus(const QString &message, bool healthy);
    void showWriteResult(
        const industrial::protocol::WriteResult &result);
    void showVisionSource(
        const industrial::monitor::vision::VisionSourceInfo &source);
    void showVisionFrame(
        const industrial::monitor::vision::VisionFrameResult &frame);
    void setVisionPlaybackState(
        industrial::monitor::vision::VisionPlaybackState state,
        const QString &message);
    void showVisionError(const QString &message);

signals:
    void connectRequested();
    void disconnectRequested();
    void writeTargetSpeedRequested(quint16 targetSpeed);
    void saveDeviceRequested(const industrial::protocol::DeviceConfig &config);
    void acknowledgeRequested(const QString &alarmId, const QString &note);
    void openVisionVideoRequested(const QString &path);
    void playVisionVideoRequested();
    void pauseVisionVideoRequested();
    void stopVisionVideoRequested();

private:
    RealtimePage *m_realtimePage = nullptr;
    DevicePage *m_devicePage = nullptr;
    AlarmPage *m_alarmPage = nullptr;
    VisionPage *m_visionPage = nullptr;
};
