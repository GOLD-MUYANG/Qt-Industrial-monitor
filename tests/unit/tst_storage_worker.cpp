#include <QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include "StorageWorker.h"

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

MeasurementSample sample(const QString &tagId,
                         double value,
                         qint64 timestampMs,
                         quint64 sequence,
                         DataQuality quality = DataQuality::Good)
{
    MeasurementSample result;
    result.deviceId = QStringLiteral("virtual-plc-1");
    result.tagId = tagId;
    result.engineeringValue = value;
    result.quality = quality;
    result.timestampUtc = QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::UTC);
    result.sequence = sequence;
    return result;
}

AlarmRecord activeAlarm()
{
    AlarmRecord alarm;
    alarm.id = QStringLiteral("alarm-1");
    alarm.ruleId = QStringLiteral("temperature-range");
    alarm.deviceId = QStringLiteral("virtual-plc-1");
    alarm.tagId = QStringLiteral("temperature");
    alarm.kind = AlarmKind::Threshold;
    alarm.severity = AlarmSeverity::Critical;
    alarm.state = AlarmState::ActiveUnacknowledged;
    alarm.message = QStringLiteral("温度超出 10..80 ℃");
    alarm.triggerValue = 86.3;
    alarm.activatedAtUtc =
        QDateTime::fromMSecsSinceEpoch(1'700'000'000'000LL, Qt::UTC);
    return alarm;
}

int scalarInt(const QString &databasePath, const QString &sql)
{
    const QString name =
        QStringLiteral("verify-%1").arg(QUuid::createUuid().toString());
    int value = -1;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(sql) && query.next()) {
                value = query.value(0).toInt();
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return value;
}

} // namespace

class StorageWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void opensConnectionInWorkerThread();
    void downSamplesMeasurementsAndUpsertsAlarmLifecycle();
    void retriesAlarmAfterTransientWriteLock();
    void retriesConnectionAfterDatabasePathBecomesAvailable();
    void rejectsAmbiguousMultipleDeviceConfiguration();
    void stopFlushesPendingMeasurements();
};

void StorageWorkerTest::initTestCase()
{
    registerProtocolMetaTypes();
    registerAlarmMetaTypes();
}

void StorageWorkerTest::rejectsAmbiguousMultipleDeviceConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("two-devices.db"));
    StorageWorker seeder;
    seeder.start(path);
    seeder.stop();

    const QString connectionName =
        QStringLiteral("second-device-%1")
            .arg(QUuid::createUuid().toString());
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                   connectionName);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY2(query.exec(QStringLiteral(
                     "INSERT INTO device "
                     "(id,name,protocol_key,host,port,unit_id,poll_interval_ms,"
                     "timeout_ms,protocol_retries,failure_threshold,enabled) "
                     "VALUES ('second','Second','modbus-tcp','127.0.0.1',1503,"
                     "1,500,800,1,3,0)")),
                 qPrintable(query.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    StorageWorker worker;
    QSignalSpy readySpy(&worker, &StorageWorker::ready);
    QSignalSpy errorSpy(&worker, &StorageWorker::storageError);
    worker.start(path);
    QCOMPARE(readySpy.count(), 0);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(errorSpy.constLast().at(0).toString().contains(
        QStringLiteral("首版仅支持一台设备")));
    worker.stop();
}

void StorageWorkerTest::retriesConnectionAfterDatabasePathBecomesAvailable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databaseDirectory =
        directory.filePath(QStringLiteral("created-later"));
    const QString path =
        QDir(databaseDirectory).filePath(QStringLiteral("monitor.db"));
    StorageWorker worker;
    QSignalSpy readySpy(&worker, &StorageWorker::ready);
    QSignalSpy errorSpy(&worker, &StorageWorker::storageError);

    worker.start(path);
    QCOMPARE(readySpy.count(), 0);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(QDir().mkpath(databaseDirectory));
    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 3'500);
    worker.stop();
}

void StorageWorkerTest::retriesAlarmAfterTransientWriteLock()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("alarm-retry.db"));
    StorageWorker worker;
    QSignalSpy alarmSpy(&worker, &StorageWorker::alarmStored);
    worker.start(path);

    const QString lockName =
        QStringLiteral("alarm-lock-%1").arg(QUuid::createUuid().toString());
    {
        auto lockDatabase =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), lockName);
        lockDatabase.setDatabaseName(path);
        QVERIFY(lockDatabase.open());
        QSqlQuery lockQuery(lockDatabase);
        QVERIFY2(lockQuery.exec(QStringLiteral("BEGIN IMMEDIATE")),
                 qPrintable(lockQuery.lastError().text()));

        worker.enqueueAlarm(activeAlarm());
        QCOMPARE(alarmSpy.count(), 0);
        QVERIFY(lockQuery.exec(QStringLiteral("COMMIT")));
        lockDatabase.close();
    }
    QSqlDatabase::removeDatabase(lockName);

    worker.flushNow();
    QCOMPARE(alarmSpy.count(), 1);
    QCOMPARE(scalarInt(path, QStringLiteral("SELECT COUNT(*) FROM alarm")), 1);
    worker.stop();
}

void StorageWorkerTest::opensConnectionInWorkerThread()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QThread thread;
    auto *worker = new StorageWorker;
    worker->moveToThread(&thread);
    QSignalSpy readySpy(worker, &StorageWorker::ready);
    QSignalSpy stoppedSpy(worker, &StorageWorker::stopped);
    connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    thread.start();

    const quintptr mainThreadId =
        reinterpret_cast<quintptr>(QThread::currentThreadId());
    QVERIFY(QMetaObject::invokeMethod(
        worker, "start", Qt::QueuedConnection,
        Q_ARG(QString, directory.filePath(QStringLiteral("thread.db")))));
    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 2'000);
    const quintptr openedThreadId = readySpy.constFirst().at(2).value<quintptr>();
    QVERIFY(openedThreadId != 0);
    QVERIFY(openedThreadId != mainThreadId);

    QVERIFY(QMetaObject::invokeMethod(worker, "stop", Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 2'000);
    thread.quit();
    QVERIFY(thread.wait(2'000));
}

void StorageWorkerTest::downSamplesMeasurementsAndUpsertsAlarmLifecycle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("storage.db"));
    StorageWorker worker;
    QSignalSpy readySpy(&worker, &StorageWorker::ready);
    QSignalSpy committedSpy(&worker, &StorageWorker::measurementsCommitted);
    QSignalSpy alarmSpy(&worker, &StorageWorker::alarmStored);
    QSignalSpy savedSpy(&worker, &StorageWorker::deviceSaved);
    worker.start(path);
    QCOMPARE(readySpy.count(), 1);
    auto config = qvariant_cast<DeviceConfig>(readySpy.constFirst().at(0));
    QCOMPARE(config.name, QStringLiteral("VirtualPLC"));
    QCOMPARE(config.protocolKey, QStringLiteral("modbus-tcp"));
    QVERIFY(config.enabled);

    config.enabled = false;
    worker.saveDevice(config);
    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(scalarInt(path,
                       QStringLiteral("SELECT enabled FROM device "
                                      "WHERE id='virtual-plc-1'")),
             0);

    worker.enqueueSamples({
        sample(QStringLiteral("temperature"), 80.0, 1'700'000'000'100LL, 1),
        sample(QStringLiteral("temperature"), 81.0, 1'700'000'000'900LL, 2),
        sample(QStringLiteral("temperature"), 82.0, 1'700'000'001'100LL, 3),
        sample(QStringLiteral("pressure"), 1.2, 1'700'000'001'200LL, 3),
        sample(QStringLiteral("voltage"), 220.0, 1'700'000'001'200LL, 3,
               DataQuality::Stale),
    });
    worker.flushNow();
    QCOMPARE(committedSpy.count(), 1);
    QCOMPARE(committedSpy.constFirst().at(0).toInt(), 3);
    QCOMPARE(scalarInt(path, QStringLiteral("SELECT COUNT(*) FROM measurement")),
             3);
    QCOMPARE(scalarInt(path,
                       QStringLiteral("SELECT CAST(value AS INTEGER) FROM measurement "
                                      "WHERE tag_id='temperature' "
                                      "ORDER BY timestamp_utc LIMIT 1")),
             81);

    auto alarm = activeAlarm();
    worker.enqueueAlarm(alarm);
    alarm.state = AlarmState::ActiveAcknowledged;
    alarm.acknowledgedAtUtc =
        QDateTime::fromMSecsSinceEpoch(1'700'000'002'000LL, Qt::UTC);
    alarm.acknowledgementNote = QStringLiteral("现场已知悉");
    worker.enqueueAlarm(alarm);
    alarm.state = AlarmState::RecoveredAcknowledged;
    alarm.recoveredAtUtc =
        QDateTime::fromMSecsSinceEpoch(1'700'000'003'000LL, Qt::UTC);
    worker.enqueueAlarm(alarm);
    QCOMPARE(alarmSpy.count(), 3);
    QCOMPARE(scalarInt(path, QStringLiteral("SELECT COUNT(*) FROM alarm")), 1);
    QCOMPARE(scalarInt(path,
                       QStringLiteral("SELECT acknowledged_at_utc IS NOT NULL "
                                      "AND recovered_at_utc IS NOT NULL "
                                      "AND acknowledgement_note='现场已知悉' "
                                      "FROM alarm WHERE id='alarm-1'")),
             1);

    worker.stop();
}

void StorageWorkerTest::stopFlushesPendingMeasurements()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("stop.db"));
    StorageWorker worker;
    worker.start(path);
    worker.enqueueSamples({sample(QStringLiteral("temperature"), 42.0,
                                  1'700'000'000'100LL, 1)});
    worker.stop();

    QCOMPARE(scalarInt(path, QStringLiteral("SELECT COUNT(*) FROM measurement")),
             1);
}

QTEST_GUILESS_MAIN(StorageWorkerTest)

#include "tst_storage_worker.moc"
