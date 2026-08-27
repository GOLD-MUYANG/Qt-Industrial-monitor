#pragma once

#include <QMainWindow>

#include "AlarmTypes.h"

#include <industrial/protocol/ProtocolTypes.h>

class AlarmPage;
class DevicePage;
class RealtimePage;

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

signals:
    void connectRequested();
    void disconnectRequested();
    void writeTargetSpeedRequested(quint16 targetSpeed);
    void saveDeviceRequested(const industrial::protocol::DeviceConfig &config);
    void acknowledgeRequested(const QString &alarmId, const QString &note);

private:
    RealtimePage *m_realtimePage = nullptr;
    DevicePage *m_devicePage = nullptr;
    AlarmPage *m_alarmPage = nullptr;
};
