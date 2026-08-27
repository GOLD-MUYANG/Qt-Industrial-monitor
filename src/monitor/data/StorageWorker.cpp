#include "StorageWorker.h"

#include "DatabaseMigrator.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QVariant>

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace
{

QVariant nullableUtcMilliseconds(const QDateTime &timestamp)
{
    return timestamp.isValid() ? QVariant(timestamp.toMSecsSinceEpoch()) : QVariant();
}

QString measurementBucketKey(const MeasurementSample &sample)
{
    return QStringLiteral("%1\x1f%2\x1f%3")
        .arg(sample.deviceId, sample.tagId,
             QString::number(sample.timestampUtc.toMSecsSinceEpoch() / 1'000));
}

} // namespace

StorageWorker::StorageWorker(QObject *parent) : QObject(parent)
{
}

void StorageWorker::start(const QString &databasePath)
{
    if (m_database.isValid() && m_database.isOpen())
    {
        emit storageError(QStringLiteral("数据库写连接已经启动"));
        return;
    }
    if (databasePath.trimmed().isEmpty())
    {
        emit storageError(QStringLiteral("数据库路径不能为空"));
        return;
    }
    m_databasePath = databasePath;

    registerProtocolMetaTypes();
    registerAlarmMetaTypes();
    m_connectionName = QStringLiteral("storage-%1").arg(QUuid::createUuid().toString());
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(databasePath);
    if (!m_database.open())
    {
        const QString error = m_database.lastError().text();
        closeDatabase();
        emit storageError(QStringLiteral("打开 SQLite 写连接失败：%1").arg(error));
        scheduleReconnect();
        return;
    }

    const auto migration = DatabaseMigrator::migrate(m_database);
    if (!migration.success)
    {
        const QString error = migration.errorMessage;
        closeDatabase();
        emit storageError(QStringLiteral("数据库迁移失败：%1").arg(error));
        scheduleReconnect();
        return;
    }

    DeviceConfig device;
    AlarmRuleList rules;
    QString errorMessage;
    if (!loadConfiguration(&device, &rules, &errorMessage))
    {
        closeDatabase();
        emit storageError(errorMessage);
        scheduleReconnect();
        return;
    }

    if (m_reconnectTimer)
    {
        m_reconnectTimer->stop();
    }

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(1'000);
    connect(m_flushTimer, &QTimer::timeout, this, &StorageWorker::flushNow);
    m_flushTimer->start();

    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setInterval(24 * 60 * 60 * 1'000);
    connect(m_cleanupTimer, &QTimer::timeout, this, &StorageWorker::cleanupExpiredMeasurements);
    m_cleanupTimer->start();
    cleanupExpiredMeasurements();

    emit ready(device, rules, reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

void StorageWorker::stop()
{
    if (m_reconnectTimer)
    {
        m_reconnectTimer->stop();
    }
    if (m_flushTimer)
    {
        m_flushTimer->stop();
    }
    if (m_cleanupTimer)
    {
        m_cleanupTimer->stop();
    }
    // 调用 flushNow() 写入剩余缓存
    flushNow();
    delete m_flushTimer;
    m_flushTimer = nullptr;
    delete m_cleanupTimer;
    m_cleanupTimer = nullptr;
    delete m_reconnectTimer;
    m_reconnectTimer = nullptr;
    closeDatabase();
    m_databasePath.clear();
    emit stopped();
}

void StorageWorker::scheduleReconnect()
{
    if (!m_reconnectTimer)
    {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        m_reconnectTimer->setInterval(2'000);
        connect(m_reconnectTimer, &QTimer::timeout, this,
                [this]()
                {
                    if (!m_databasePath.isEmpty())
                    {
                        start(m_databasePath);
                    }
                });
    }
    m_reconnectTimer->start();
}

void StorageWorker::enqueueSamples(const SampleBatch &samples)
{
    if (!m_database.isOpen())
    {
        emit storageError(QStringLiteral("数据库写连接未启动，无法保存测量值"));
        return;
    }
    for (const auto &sample : samples)
    {
        // 再做一次校验
        if (sample.quality != DataQuality::Good || sample.deviceId.isEmpty() ||
            sample.tagId.isEmpty() || !sample.timestampUtc.isValid() ||
            sample.timestampUtc.timeSpec() != Qt::UTC)
        {
            continue;
        }
        const QString key = measurementBucketKey(sample);
        const auto existing = m_pendingMeasurements.constFind(key);
        if (existing == m_pendingMeasurements.constEnd() ||
            existing->timestampUtc <= sample.timestampUtc)
        {
            m_pendingMeasurements.insert(key, sample);
        }
        if (m_pendingMeasurements.size() > 10'000)
        {
            auto oldest = m_pendingMeasurements.begin();
            for (auto it = m_pendingMeasurements.begin(); it != m_pendingMeasurements.end(); ++it)
            {
                if (it->timestampUtc < oldest->timestampUtc)
                {
                    oldest = it;
                }
            }
            m_pendingMeasurements.erase(oldest);
            emit storageError(
                QStringLiteral("StorageOverflow：测量重试缓冲超过 10000 行，已丢弃最旧候选"));
        }
    }
    if (m_pendingMeasurements.size() >= 50)
    {
        flushNow();
    }
}

void StorageWorker::enqueueAlarm(const AlarmRecord &alarm)
{
    if (!m_database.isOpen())
    {
        emit storageError(QStringLiteral("数据库写连接未启动，无法保存报警"));
        return;
    }
    if (alarm.id.isEmpty() || alarm.ruleId.isEmpty() || !alarm.activatedAtUtc.isValid())
    {
        emit storageError(QStringLiteral("报警 ID、规则 ID 或激活时间无效"));
        return;
    }

    m_pendingAlarms.insert(alarm.id, alarm);
    if (m_pendingAlarms.size() > 1'000)
    {
        auto oldest = m_pendingAlarms.begin();
        for (auto it = m_pendingAlarms.begin(); it != m_pendingAlarms.end(); ++it)
        {
            if (it->activatedAtUtc < oldest->activatedAtUtc)
            {
                oldest = it;
            }
        }
        m_pendingAlarms.erase(oldest);
        emit storageError(
            QStringLiteral("StorageOverflow：报警重试缓冲超过 1000 条，已丢弃最旧报警"));
    }
    flushPendingAlarms();
}

void StorageWorker::saveDevice(const DeviceConfig &config)
{
    if (!m_database.isOpen())
    {
        emit storageError(QStringLiteral("数据库写连接未启动，无法保存设备"));
        return;
    }
    if (config.id.isEmpty() || config.name.trimmed().isEmpty() ||
        config.protocolKey.trimmed().isEmpty() || config.host.trimmed().isEmpty() ||
        config.port == 0 || config.unitId < 1 || config.unitId > 247 ||
        config.pollIntervalMs < 50 || config.timeoutMs < 1)
    {
        emit storageError(QStringLiteral("设备配置无效"));
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral("UPDATE device SET name=:name,protocol_key=:protocol,enabled=:enabled,"
                       "host=:host,port=:port,unit_id=:unit,"
                       "poll_interval_ms=:poll,timeout_ms=:timeout,"
                       "protocol_retries=:retries,failure_threshold=:failures WHERE id=:id"));
    query.bindValue(QStringLiteral(":name"), config.name);
    query.bindValue(QStringLiteral(":protocol"), config.protocolKey);
    query.bindValue(QStringLiteral(":enabled"), config.enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":host"), config.host);
    query.bindValue(QStringLiteral(":port"), config.port);
    query.bindValue(QStringLiteral(":unit"), config.unitId);
    query.bindValue(QStringLiteral(":poll"), config.pollIntervalMs);
    query.bindValue(QStringLiteral(":timeout"), config.timeoutMs);
    query.bindValue(QStringLiteral(":retries"), config.protocolRetries);
    query.bindValue(QStringLiteral(":failures"), config.consecutiveFailureLimit);
    query.bindValue(QStringLiteral(":id"), config.id);
    if (!query.exec() || query.numRowsAffected() != 1)
    {
        emit storageError(QStringLiteral("保存设备配置失败：%1").arg(query.lastError().text()));
        return;
    }
    emit deviceSaved(config);
}

void StorageWorker::flushNow()
{
    flushPendingAlarms();
    if (!m_database.isOpen() || m_pendingMeasurements.isEmpty())
    {
        return;
    }
    if (!m_database.transaction())
    {
        emit storageError(
            QStringLiteral("开始测量事务失败：%1").arg(m_database.lastError().text()));
        return;
    }

    QSqlQuery query(m_database);
    if (!query.prepare(QStringLiteral("INSERT INTO measurement "
                                      "(device_id,tag_id,value,quality,timestamp_utc,sequence) "
                                      "VALUES (:device,:tag,:value,:quality,:time,:sequence)")))
    {
        m_database.rollback();
        emit storageError(query.lastError().text());
        return;
    }

    int inserted = 0;
    for (const auto &sample : std::as_const(m_pendingMeasurements))
    {
        query.bindValue(QStringLiteral(":device"), sample.deviceId);
        query.bindValue(QStringLiteral(":tag"), sample.tagId);
        query.bindValue(QStringLiteral(":value"), sample.engineeringValue);
        query.bindValue(QStringLiteral(":quality"), static_cast<int>(sample.quality));
        query.bindValue(QStringLiteral(":time"), sample.timestampUtc.toMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":sequence"),
                        QVariant::fromValue<qulonglong>(sample.sequence));
        if (!query.exec())
        {
            m_database.rollback();
            emit storageError(QStringLiteral("写入测量值失败：%1").arg(query.lastError().text()));
            return;
        }
        ++inserted;
    }
    if (!m_database.commit())
    {
        m_database.rollback();
        emit storageError(
            QStringLiteral("提交测量事务失败：%1").arg(m_database.lastError().text()));
        return;
    }
    m_pendingMeasurements.clear();
    emit measurementsCommitted(inserted);
}

void StorageWorker::flushPendingAlarms()
{
    if (!m_database.isOpen() || m_pendingAlarms.isEmpty())
    {
        return;
    }
    if (!m_database.transaction())
    {
        emit storageError(
            QStringLiteral("开始报警事务失败：%1").arg(m_database.lastError().text()));
        return;
    }

    QSqlQuery query(m_database);
    if (!query.prepare(
            QStringLiteral("INSERT INTO alarm "
                           "(id,rule_id,device_id,tag_id,message,severity,trigger_value,"
                           "activated_at_utc,acknowledged_at_utc,recovered_at_utc,"
                           "acknowledgement_note) "
                           "VALUES (:id,:rule,:device,:tag,:message,:severity,:value,:activated,"
                           ":acknowledged,:recovered,:note) "
                           "ON CONFLICT(id) DO UPDATE SET "
                           "message=excluded.message,severity=excluded.severity,"
                           "trigger_value=excluded.trigger_value,"
                           "acknowledged_at_utc=excluded.acknowledged_at_utc,"
                           "recovered_at_utc=excluded.recovered_at_utc,"
                           "acknowledgement_note=excluded.acknowledgement_note")))
    {
        m_database.rollback();
        emit storageError(query.lastError().text());
        return;
    }

    const QStringList alarmIds = m_pendingAlarms.keys();
    for (const auto &alarmId : alarmIds)
    {
        const auto alarm = m_pendingAlarms.value(alarmId);
        query.bindValue(QStringLiteral(":id"), alarm.id);
        query.bindValue(QStringLiteral(":rule"), alarm.ruleId);
        query.bindValue(QStringLiteral(":device"), alarm.deviceId);
        query.bindValue(QStringLiteral(":tag"),
                        alarm.tagId.isEmpty() ? QVariant() : QVariant(alarm.tagId));
        query.bindValue(QStringLiteral(":message"), alarm.message);
        query.bindValue(QStringLiteral(":severity"), static_cast<int>(alarm.severity));
        query.bindValue(QStringLiteral(":value"), alarm.triggerValue);
        query.bindValue(QStringLiteral(":activated"), alarm.activatedAtUtc.toMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":acknowledged"),
                        nullableUtcMilliseconds(alarm.acknowledgedAtUtc));
        query.bindValue(QStringLiteral(":recovered"),
                        nullableUtcMilliseconds(alarm.recoveredAtUtc));
        query.bindValue(QStringLiteral(":note"), alarm.acknowledgementNote.isEmpty()
                                                     ? QVariant()
                                                     : QVariant(alarm.acknowledgementNote));
        if (!query.exec())
        {
            m_database.rollback();
            emit storageError(QStringLiteral("保存报警失败：%1").arg(query.lastError().text()));
            return;
        }
    }
    if (!m_database.commit())
    {
        m_database.rollback();
        emit storageError(
            QStringLiteral("提交报警事务失败：%1").arg(m_database.lastError().text()));
        return;
    }
    for (const auto &alarmId : alarmIds)
    {
        m_pendingAlarms.remove(alarmId);
        emit alarmStored(alarmId);
    }
}

void StorageWorker::cleanupExpiredMeasurements()
{
    if (!m_database.isOpen())
    {
        return;
    }
    const qint64 cutoff = QDateTime::currentDateTimeUtc().addDays(-7).toMSecsSinceEpoch();
    while (true)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "DELETE FROM measurement WHERE id IN "
            "(SELECT id FROM measurement WHERE timestamp_utc < :cutoff LIMIT 1000)"));
        query.bindValue(QStringLiteral(":cutoff"), cutoff);
        if (!query.exec())
        {
            emit storageError(
                QStringLiteral("清理过期测量值失败：%1").arg(query.lastError().text()));
            return;
        }
        if (query.numRowsAffected() < 1'000)
        {
            return;
        }
    }
}

bool StorageWorker::loadConfiguration(DeviceConfig *device,
                                      AlarmRuleList *rules,
                                      QString *errorMessage)
{
    QSqlQuery countQuery(m_database);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM device")) || !countQuery.next())
    {
        *errorMessage = QStringLiteral("读取设备数量失败：%1").arg(countQuery.lastError().text());
        return false;
    }
    if (countQuery.value(0).toInt() != 1)
    {
        *errorMessage = QStringLiteral("首版仅支持一台设备，当前数据库包含 %1 台")
                            .arg(countQuery.value(0).toInt());
        return false;
    }

    QSqlQuery deviceQuery(m_database);
    if (!deviceQuery.exec(
            QStringLiteral("SELECT id,name,protocol_key,host,port,unit_id,poll_interval_ms,"
                           "timeout_ms,protocol_retries,failure_threshold,enabled FROM device "
                           "ORDER BY id LIMIT 1")) ||
        !deviceQuery.next())
    {
        *errorMessage = QStringLiteral("读取默认设备失败：%1").arg(deviceQuery.lastError().text());
        return false;
    }
    device->id = deviceQuery.value(0).toString();
    device->name = deviceQuery.value(1).toString();
    device->protocolKey = deviceQuery.value(2).toString();
    device->host = deviceQuery.value(3).toString();
    device->port = static_cast<quint16>(deviceQuery.value(4).toUInt());
    device->unitId = deviceQuery.value(5).toInt();
    device->pollIntervalMs = deviceQuery.value(6).toInt();
    device->timeoutMs = deviceQuery.value(7).toInt();
    device->protocolRetries = deviceQuery.value(8).toInt();
    device->consecutiveFailureLimit = deviceQuery.value(9).toInt();
    device->enabled = deviceQuery.value(10).toBool();

    QSqlQuery ruleQuery(m_database);
    if (!ruleQuery.exec(QStringLiteral(
            "SELECT id,device_id,tag_id,rule_type,lower_limit,upper_limit,"
            "hysteresis,activation_samples,recovery_samples,severity,enabled,message "
            "FROM alarm_rule ORDER BY id")))
    {
        *errorMessage = QStringLiteral("读取报警规则失败：%1").arg(ruleQuery.lastError().text());
        return false;
    }
    while (ruleQuery.next())
    {
        AlarmRule rule;
        rule.id = ruleQuery.value(0).toString();
        rule.deviceId = ruleQuery.value(1).toString();
        rule.tagId = ruleQuery.value(2).toString();
        rule.kind = static_cast<AlarmKind>(ruleQuery.value(3).toInt());
        rule.hasLowerLimit = !ruleQuery.value(4).isNull();
        rule.lowerLimit = ruleQuery.value(4).toDouble();
        rule.hasUpperLimit = !ruleQuery.value(5).isNull();
        rule.upperLimit = ruleQuery.value(5).toDouble();
        rule.hysteresis = ruleQuery.value(6).toDouble();
        rule.activationSamples = ruleQuery.value(7).toInt();
        rule.recoverySamples = ruleQuery.value(8).toInt();
        rule.severity = static_cast<AlarmSeverity>(ruleQuery.value(9).toInt());
        rule.enabled = ruleQuery.value(10).toBool();
        rule.message = ruleQuery.value(11).toString();
        rules->append(rule);
    }
    return true;
}

void StorageWorker::closeDatabase()
{
    if (!m_database.isValid())
    {
        return;
    }
    const QString connectionName = m_connectionName;
    m_database.close();
    m_database = QSqlDatabase();
    m_connectionName.clear();
    QSqlDatabase::removeDatabase(connectionName);
}
