#pragma once

#include "AlarmTypes.h"

#include <QHash>
#include <QObject>
#include <QSqlDatabase>

#include <industrial/protocol/ProtocolTypes.h>

class QTimer;

class StorageWorker final : public QObject
{
    Q_OBJECT

public:
    explicit StorageWorker(QObject *parent = nullptr);

public slots:
    void start(const QString &databasePath);
    void stop();
    void enqueueSamples(const industrial::protocol::SampleBatch &samples);
    void enqueueAlarm(const industrial::monitor::AlarmRecord &alarm);
    void saveDevice(const industrial::protocol::DeviceConfig &config);
    void flushNow();
    void cleanupExpiredMeasurements();

signals:
    void ready(const industrial::protocol::DeviceConfig &device,
               const industrial::monitor::AlarmRuleList &rules,
               quintptr threadId);
    void storageError(const QString &message);
    void measurementsCommitted(int rowCount);
    void alarmStored(const QString &alarmId);
    void deviceSaved(const industrial::protocol::DeviceConfig &device);
    void stopped();

private:
    bool loadConfiguration(industrial::protocol::DeviceConfig *device,
                           industrial::monitor::AlarmRuleList *rules,
                           QString *errorMessage);
    void flushPendingAlarms();
    void scheduleReconnect();
    void closeDatabase();

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
    QTimer *m_flushTimer = nullptr;
    QTimer *m_cleanupTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    // 数据会先存放在hash表里当作缓存，等数量足够或者定时器到了一秒，再写入数据库，避免多次开启SQLite事务
    QHash<QString, industrial::protocol::MeasurementSample> m_pendingMeasurements;
    QHash<QString, industrial::monitor::AlarmRecord> m_pendingAlarms;
};
