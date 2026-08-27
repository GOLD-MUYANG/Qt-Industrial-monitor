#include "QmlApplicationFacade.h"

#include "ApplicationController.h"
#include "QmlAlarmModel.h"
#include "QmlDeviceViewModel.h"
#include "QmlProtocolModel.h"
#include "QmlRealtimeModel.h"
#include "QmlTrendModel.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

DeviceConfig configuredDevice()
{
    DeviceConfig device;
    device.id = QStringLiteral("device-1");
    device.name = QStringLiteral("一号设备");
    device.protocolKey = QStringLiteral("modbus-tcp");
    device.host = QStringLiteral("127.0.0.1");
    device.port = 1502;
    device.unitId = 1;
    device.pollIntervalMs = 500;
    device.timeoutMs = 800;
    device.protocolRetries = 3;
    device.consecutiveFailureLimit = 5;
    return device;
}

} // namespace

class QmlApplicationFacadeTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void projectsControllerStateIntoModels();
    void validatesSimpleInputsBeforeForwarding();
    void rejectsConnectionWhenDisabledOrAlreadyActive();
    void preservesIdentityAndPolicyWhenSavingDevice();
    void tracksBusyStateUntilAsynchronousResult();

private:
    QTemporaryDir m_directory;
};

void QmlApplicationFacadeTest::initTestCase()
{
    registerProtocolMetaTypes();
    registerAlarmMetaTypes();
    QVERIFY(m_directory.isValid());
}

void QmlApplicationFacadeTest::projectsControllerStateIntoModels()
{
    ApplicationController controller({}, m_directory.filePath(QStringLiteral("state.db")));
    QmlApplicationFacade facade(&controller);
    const auto device = configuredDevice();

    emit controller.protocolAvailable(
        {QStringLiteral("modbus-tcp"), QStringLiteral("Modbus TCP"), 1, {}});
    emit controller.deviceConfigChanged(device);
    emit controller.initialized(device);

    QVERIFY(facade.initialized());
    QCOMPARE(facade.protocolModel()->rowCount(), 1);
    QCOMPARE(facade.deviceModel()->protocolName(), QStringLiteral("Modbus TCP"));
    QCOMPARE(facade.deviceModel()->deviceId(), device.id);

    RealtimeSnapshot point;
    point.deviceId = device.id;
    point.tagId = QStringLiteral("temperature");
    point.current = 42.5;
    point.minimum = 40.0;
    point.maximum = 45.0;
    point.average = 42.0;
    point.quality = DataQuality::Good;
    point.timestampUtc = QDateTime::currentDateTimeUtc();
    emit controller.snapshotsReady({point});
    QCOMPARE(facade.realtimeModel()->data(facade.realtimeModel()->index(0),
                                          QmlRealtimeModel::CurrentValueRole)
                 .toDouble(),
             42.5);
    QCOMPARE(facade.trendModel()->rowCount(), 1);
    QVERIFY(facade.deviceModel()->lastCommunicationText()
            != QStringLiteral("尚无数据"));

    AlarmRecord alarm;
    alarm.id = QStringLiteral("alarm-1");
    alarm.deviceId = device.id;
    alarm.state = AlarmState::ActiveUnacknowledged;
    alarm.activatedAtUtc = QDateTime::currentDateTimeUtc();
    emit controller.alarmChanged(alarm);
    QCOMPARE(facade.activeAlarmModel()->rowCount(), 1);
    QCOMPARE(facade.alarmHistoryModel()->rowCount(), 1);

    emit controller.storageStatusChanged(QStringLiteral("SQLite 在线"), true);
    QCOMPARE(facade.statusMessage(), QStringLiteral("SQLite 在线"));
    QVERIFY(facade.statusHealthy());
    emit controller.fatalError(QStringLiteral("初始化失败"));
    QCOMPARE(facade.statusMessage(), QStringLiteral("初始化失败"));
    QVERIFY(!facade.statusHealthy());
}

void QmlApplicationFacadeTest::validatesSimpleInputsBeforeForwarding()
{
    ApplicationController controller({}, m_directory.filePath(QStringLiteral("validation.db")));
    QmlApplicationFacade facade(&controller);
    const auto device = configuredDevice();
    emit controller.protocolAvailable(
        {QStringLiteral("modbus-tcp"), QStringLiteral("Modbus TCP"), 1, {}});
    emit controller.initialized(device);
    QSignalSpy writeSpy(&facade, &QmlApplicationFacade::writeTargetSpeedRequested);
    QSignalSpy saveSpy(&facade, &QmlApplicationFacade::saveDeviceRequested);
    QSignalSpy acknowledgeSpy(&facade,
                              &QmlApplicationFacade::acknowledgeAlarmRequested);

    facade.writeTargetSpeed(-1);
    facade.writeTargetSpeed(65'536);
    QCOMPARE(writeSpy.size(), 0);
    QVERIFY(facade.statusMessage().contains(QStringLiteral("0 到 65535")));

    facade.saveDevice(QStringLiteral("modbus-tcp"), QString(), 1502, 1, 500, 800, true);
    facade.saveDevice(QStringLiteral("missing"), QStringLiteral("127.0.0.1"), 1502,
                      1, 500, 800, true);
    facade.saveDevice(QStringLiteral("modbus-tcp"), QStringLiteral("127.0.0.1"),
                      65'536, 1, 500, 800, true);
    facade.saveDevice(QStringLiteral("modbus-tcp"), QStringLiteral("127.0.0.1"),
                      1502, 248, 500, 800, true);
    facade.saveDevice(QStringLiteral("modbus-tcp"), QStringLiteral("127.0.0.1"),
                      1502, 1, 49, 800, true);
    facade.saveDevice(QStringLiteral("modbus-tcp"), QStringLiteral("127.0.0.1"),
                      1502, 1, 500, 0, true);
    QCOMPARE(saveSpy.size(), 0);
    QVERIFY(!facade.statusHealthy());

    facade.acknowledgeAlarm(QStringLiteral("missing-alarm"), QString());
    QCOMPARE(acknowledgeSpy.size(), 0);
    QVERIFY(facade.statusMessage().contains(QStringLiteral("可确认")));
}

void QmlApplicationFacadeTest::rejectsConnectionWhenDisabledOrAlreadyActive()
{
    ApplicationController controller({}, m_directory.filePath(QStringLiteral("connect.db")));
    QmlApplicationFacade facade(&controller);
    auto device = configuredDevice();
    device.enabled = false;
    emit controller.initialized(device);
    QSignalSpy connectSpy(&facade,
                          &QmlApplicationFacade::connectDeviceRequested);

    facade.connectDevice();
    QCOMPARE(connectSpy.size(), 0);
    QVERIFY(facade.statusMessage().contains(QStringLiteral("停用")));
    QVERIFY(!facade.commandBusy());

    device.enabled = true;
    emit controller.deviceConfigChanged(device);
    emit controller.deviceStateChanged(
        {device.id, ConnectionState::Online, QStringLiteral("在线")});
    facade.connectDevice();
    QCOMPARE(connectSpy.size(), 0);
    QVERIFY(facade.statusMessage().contains(QStringLiteral("当前连接状态")));
    QVERIFY(!facade.commandBusy());
}

void QmlApplicationFacadeTest::preservesIdentityAndPolicyWhenSavingDevice()
{
    ApplicationController controller({}, m_directory.filePath(QStringLiteral("save.db")));
    QmlApplicationFacade facade(&controller);
    const auto device = configuredDevice();
    emit controller.protocolAvailable(
        {QStringLiteral("modbus-tcp"), QStringLiteral("Modbus TCP"), 1, {}});
    emit controller.initialized(device);
    QSignalSpy saveSpy(&facade, &QmlApplicationFacade::saveDeviceRequested);

    facade.saveDevice(QStringLiteral("modbus-tcp"), QStringLiteral("192.0.2.20"),
                      2502, 9, 900, 1500, false);

    QCOMPARE(saveSpy.size(), 1);
    const auto saved = qvariant_cast<DeviceConfig>(saveSpy.takeFirst().at(0));
    QCOMPARE(saved.id, device.id);
    QCOMPARE(saved.name, device.name);
    QCOMPARE(saved.protocolRetries, device.protocolRetries);
    QCOMPARE(saved.consecutiveFailureLimit, device.consecutiveFailureLimit);
    QCOMPARE(saved.reconnectDelaysMs, device.reconnectDelaysMs);
    QCOMPARE(saved.host, QStringLiteral("192.0.2.20"));
    QCOMPARE(saved.port, 2502);
    QCOMPARE(saved.unitId, 9);
    QCOMPARE(saved.pollIntervalMs, 900);
    QCOMPARE(saved.timeoutMs, 1500);
    QCOMPARE(saved.enabled, false);
    QVERIFY(facade.commandBusy());

    emit controller.deviceConfigChanged(saved);
    QVERIFY(!facade.commandBusy());
    QCOMPARE(facade.deviceModel()->host(), saved.host);
}

void QmlApplicationFacadeTest::tracksBusyStateUntilAsynchronousResult()
{
    ApplicationController controller({}, m_directory.filePath(QStringLiteral("busy.db")));
    QmlApplicationFacade facade(&controller);
    const auto device = configuredDevice();
    emit controller.initialized(device);
    QSignalSpy connectSpy(&facade, &QmlApplicationFacade::connectDeviceRequested);
    QSignalSpy acknowledgeSpy(&facade, &QmlApplicationFacade::acknowledgeAlarmRequested);

    facade.connectDevice();
    QCOMPARE(connectSpy.size(), 1);
    QVERIFY(facade.commandBusy());
    AlarmRecord unrelatedAlarm;
    unrelatedAlarm.id = QStringLiteral("unrelated-alarm");
    unrelatedAlarm.state = AlarmState::ActiveUnacknowledged;
    emit controller.alarmChanged(unrelatedAlarm);
    // 无关报警更新不能提前结束正在进行的连接命令。
    QVERIFY(facade.commandBusy());
    emit controller.deviceStateChanged(
        {device.id, ConnectionState::Connecting, QStringLiteral("连接中")});
    QVERIFY(facade.commandBusy());
    emit controller.deviceStateChanged(
        {device.id, ConnectionState::Online, QStringLiteral("在线")});
    QVERIFY(!facade.commandBusy());

    AlarmRecord alarmToAcknowledge;
    alarmToAcknowledge.id = QStringLiteral("alarm-1");
    alarmToAcknowledge.state = AlarmState::ActiveUnacknowledged;
    emit controller.alarmChanged(alarmToAcknowledge);
    facade.acknowledgeAlarm(QStringLiteral("alarm-1"), QStringLiteral("值班员确认"));
    QCOMPARE(acknowledgeSpy.size(), 1);
    QVERIFY(facade.commandBusy());
    // 同一报警的未确认重复通知不是确认命令的完成结果。
    emit controller.alarmChanged(alarmToAcknowledge);
    QVERIFY(facade.commandBusy());
    AlarmRecord acknowledged;
    acknowledged.id = QStringLiteral("alarm-1");
    acknowledged.state = AlarmState::ActiveAcknowledged;
    emit controller.alarmChanged(acknowledged);
    QVERIFY(!facade.commandBusy());
}

QTEST_GUILESS_MAIN(QmlApplicationFacadeTest)

#include "tst_qml_application_facade.moc"
