#include <QtTest>

#include <QHostAddress>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>

#include "ApplicationController.h"
#include "RegisterBank.h"
#include "VirtualPlcServer.h"

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

quint16 availablePort()
{
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, 0) ? probe.serverPort() : 0;
}

bool hasAlarm(const QSignalSpy &spy,
              AlarmKind kind,
              AlarmState state,
              const QString &alarmId = {})
{
    for (const auto &arguments : spy) {
        const auto alarm = qvariant_cast<AlarmRecord>(arguments.at(0));
        if (alarm.kind == kind && alarm.state == state
            && (alarmId.isEmpty() || alarm.id == alarmId)) {
            return true;
        }
    }
    return false;
}

bool hasNewSessionSequence(const QSignalSpy &spy, int startIndex)
{
    for (int index = startIndex; index < spy.size(); ++index) {
        const auto snapshots =
            qvariant_cast<RealtimeSnapshotBatch>(spy.at(index).at(0));
        for (const auto &snapshot : snapshots) {
            if (snapshot.sequence > 0 && snapshot.sequence <= 2) {
                return true;
            }
        }
    }
    return false;
}

int maximumSnapshotSequence(const QSignalSpy &spy)
{
    quint64 maximum = 0;
    for (const auto &arguments : spy) {
        const auto snapshots =
            qvariant_cast<RealtimeSnapshotBatch>(arguments.at(0));
        for (const auto &snapshot : snapshots) {
            maximum = qMax(maximum, snapshot.sequence);
        }
    }
    return static_cast<int>(maximum);
}

QString alarmId(const QSignalSpy &spy,
                AlarmKind kind,
                AlarmState state)
{
    for (const auto &arguments : spy) {
        const auto alarm = qvariant_cast<AlarmRecord>(arguments.at(0));
        if (alarm.kind == kind && alarm.state == state) {
            return alarm.id;
        }
    }
    return {};
}

int scalarInt(const QString &path, const QString &sql)
{
    const QString connectionName =
        QStringLiteral("closed-loop-check-%1")
            .arg(QUuid::createUuid().toString());
    int result = -1;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                   connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(sql) && query.next()) {
                result = query.value(0).toInt();
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return result;
}

} // namespace

class WeekThreeClosedLoopTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsHighTemperatureAndCommunicationAlarmLifecycle();
};

void WeekThreeClosedLoopTest::persistsHighTemperatureAndCommunicationAlarmLifecycle()
{
    const quint16 port = availablePort();
    QVERIFY(port != 0);
    VirtualPlcServer server;
    QVERIFY(server.start(QHostAddress::LocalHost, port, 1));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath =
        directory.filePath(QStringLiteral("closed-loop.db"));
    ApplicationController controller(QStringLiteral(MODBUS_PLUGIN_DIR),
                                     databasePath);
    QSignalSpy initializedSpy(&controller, &ApplicationController::initialized);
    QSignalSpy snapshotSpy(&controller, &ApplicationController::snapshotsReady);
    QSignalSpy alarmSpy(&controller, &ApplicationController::alarmChanged);
    QSignalSpy fatalSpy(&controller, &ApplicationController::fatalError);
    QSignalSpy deviceSpy(&controller,
                         &ApplicationController::deviceConfigChanged);
    QSignalSpy writeSpy(&controller, &ApplicationController::writeFinished);

    QVERIFY(controller.start());
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 3'000);
    QCOMPARE(fatalSpy.count(), 0);

    auto config =
        qvariant_cast<DeviceConfig>(initializedSpy.constFirst().at(0));
    config.port = port;
    const int deviceEventsBeforeSave = deviceSpy.count();
    controller.applyDeviceConfig(config);
    QTRY_VERIFY_WITH_TIMEOUT(deviceSpy.count() > deviceEventsBeforeSave, 2'000);
    controller.connectDevice();
    QTRY_VERIFY_WITH_TIMEOUT(snapshotSpy.count() >= 3, 4'000);

    const quint16 speedBeforeWrite =
        server.registerBank().value(RegisterBank::Speed);
    controller.writeTargetSpeed(1'800);
    QTRY_COMPARE_WITH_TIMEOUT(writeSpy.count(), 1, 3'000);
    const auto writeResult =
        qvariant_cast<WriteResult>(writeSpy.constFirst().at(0));
    QVERIFY2(writeResult.success, qPrintable(writeResult.errorMessage));
    QTRY_COMPARE_WITH_TIMEOUT(
        server.registerBank().value(RegisterBank::TargetSpeed),
        quint16(1'800), 2'000);
    QTRY_VERIFY_WITH_TIMEOUT(
        server.registerBank().value(RegisterBank::Speed) > speedBeforeWrite,
        2'000);

    // 保存在线设备配置会有序重建设备 Worker；新会话序号从 1 开始，
    // DataPipeline 必须把它视为新的会话边界而不是乱序数据。
    QTest::qWait(200);
    const int snapshotsBeforeRestart = snapshotSpy.count();
    const int deviceEventsBeforeRestart = deviceSpy.count();
    config.pollIntervalMs = 550;
    controller.applyDeviceConfig(config);
    QTRY_VERIFY_WITH_TIMEOUT(deviceSpy.count() > deviceEventsBeforeRestart,
                             2'000);
    QTRY_VERIFY_WITH_TIMEOUT(snapshotSpy.count() > snapshotsBeforeRestart,
                             4'000);
    QTRY_VERIFY_WITH_TIMEOUT(
        hasNewSessionSequence(snapshotSpy, snapshotsBeforeRestart), 4'000);

    QVERIFY(server.setHighTemperatureEnabled(true));
    QTRY_VERIFY_WITH_TIMEOUT(
        hasAlarm(alarmSpy, AlarmKind::Threshold,
                 AlarmState::ActiveUnacknowledged),
        5'000);
    const QString temperatureAlarmId =
        alarmId(alarmSpy, AlarmKind::Threshold,
                AlarmState::ActiveUnacknowledged);
    QVERIFY(!temperatureAlarmId.isEmpty());

    QVERIFY(server.setHighTemperatureEnabled(false));
    QTRY_VERIFY_WITH_TIMEOUT(
        hasAlarm(alarmSpy, AlarmKind::Threshold,
                 AlarmState::RecoveredUnacknowledged,
                 temperatureAlarmId),
        5'000);

    const int snapshotsBeforeOutage = snapshotSpy.count();
    server.stop();
    QTRY_VERIFY_WITH_TIMEOUT(
        hasAlarm(alarmSpy, AlarmKind::Communication,
                 AlarmState::ActiveUnacknowledged),
        6'000);
    const QString communicationAlarmId =
        alarmId(alarmSpy, AlarmKind::Communication,
                AlarmState::ActiveUnacknowledged);
    QVERIFY(!communicationAlarmId.isEmpty());

    QVERIFY(server.start(QHostAddress::LocalHost, port, 1));
    QTRY_VERIFY_WITH_TIMEOUT(
        hasAlarm(alarmSpy, AlarmKind::Communication,
                 AlarmState::RecoveredUnacknowledged,
                 communicationAlarmId),
        6'000);
    QTRY_VERIFY_WITH_TIMEOUT(snapshotSpy.count() > snapshotsBeforeOutage, 4'000);

    QVERIFY(controller.shutdown(3'000));
    QVERIFY(!controller.isRunning());
    // 数据线程栅栏可先把最后一批写入 SQLite，而主线程的快照转发事件
    // 尚未派发，因此持久化序号允许领先，但绝不能落后于 UI 已见序号。
    QVERIFY(scalarInt(databasePath,
                      QStringLiteral("SELECT MAX(sequence) FROM measurement"))
            >= maximumSnapshotSequence(snapshotSpy));
    QCOMPARE(scalarInt(databasePath,
                       QStringLiteral("SELECT COUNT(*) FROM measurement")) > 0,
             true);
    QCOMPARE(scalarInt(databasePath,
                       QStringLiteral("SELECT COUNT(*) FROM alarm")),
             2);
    QCOMPARE(scalarInt(databasePath,
                       QStringLiteral("SELECT COUNT(*) FROM alarm "
                                      "WHERE recovered_at_utc IS NOT NULL")),
             2);
}

QTEST_GUILESS_MAIN(WeekThreeClosedLoopTest)

#include "tst_week_three_closed_loop.moc"
