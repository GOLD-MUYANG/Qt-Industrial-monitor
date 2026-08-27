#include "ApplicationController.h"
#include "QmlAlarmModel.h"
#include "QmlApplicationFacade.h"
#include "QmlDeviceViewModel.h"
#include "QmlRealtimeModel.h"
#include "QmlTrendModel.h"
#include "RegisterBank.h"
#include "VirtualPlcServer.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

quint16 availablePort()
{
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, 0) ? probe.serverPort() : 0;
}

int alarmRowForTag(const QmlAlarmModel *model, const QString &tagText)
{
    for (int row = 0; row < model->rowCount(); ++row)
    {
        if (model->data(model->index(row), QmlAlarmModel::TagTextRole)
                .toString() == tagText)
        {
            return row;
        }
    }
    return -1;
}

} // namespace

class QmlBackendClosedLoopTest final : public QObject
{
    Q_OBJECT

private slots:
    void projectsRealModbusLifecycleIntoQmlModels();
};

void QmlBackendClosedLoopTest::projectsRealModbusLifecycleIntoQmlModels()
{
    const quint16 port = availablePort();
    QVERIFY(port != 0);
    VirtualPlcServer server;
    QVERIFY(server.start(QHostAddress::LocalHost, port, 1));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationController controller(
        QStringLiteral(MODBUS_PLUGIN_DIR),
        directory.filePath(QStringLiteral("qml-backend.db")));
    QmlApplicationFacade facade(&controller);
    QSignalSpy fatalSpy(&controller, &ApplicationController::fatalError);

    QVERIFY(controller.start());
    QTRY_VERIFY_WITH_TIMEOUT(facade.initialized(), 3'000);
    QCOMPARE(fatalSpy.size(), 0);
    QCOMPARE(facade.protocolModel()->rowCount(), 1);

    facade.saveDevice(QStringLiteral("modbus-tcp"),
                      QStringLiteral("127.0.0.1"), port, 1, 500, 800, true);
    QTRY_VERIFY_WITH_TIMEOUT(!facade.commandBusy(), 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(facade.deviceModel()->port(), int(port), 2'000);

    facade.connectDevice();
    QTRY_COMPARE_WITH_TIMEOUT(
        facade.deviceModel()->connectionState(),
        static_cast<int>(ConnectionState::Online), 4'000);
    QTRY_VERIFY_WITH_TIMEOUT(
        facade.realtimeModel()
            ->data(facade.realtimeModel()->index(0),
                   QmlRealtimeModel::CurrentValueRole)
            .isValid(),
        4'000);
    QTRY_VERIFY_WITH_TIMEOUT(facade.trendModel()->rowCount() >= 3, 4'000);

    facade.writeTargetSpeed(1'800);
    QTRY_COMPARE_WITH_TIMEOUT(
        server.registerBank().value(RegisterBank::TargetSpeed),
        quint16(1'800), 3'000);
    QTRY_VERIFY_WITH_TIMEOUT(
        facade.statusMessage().contains(QStringLiteral("写入成功")), 3'000);

    QVERIFY(server.setHighTemperatureEnabled(true));
    QTRY_VERIFY_WITH_TIMEOUT(
        alarmRowForTag(facade.activeAlarmModel(),
                       QStringLiteral("temperature")) >= 0,
        5'000);
    int temperatureRow = alarmRowForTag(
        facade.activeAlarmModel(), QStringLiteral("temperature"));
    const QString alarmId = facade.activeAlarmModel()
                                ->data(facade.activeAlarmModel()->index(temperatureRow),
                                       QmlAlarmModel::AlarmIdRole)
                                .toString();
    QVERIFY(!alarmId.isEmpty());

    facade.acknowledgeAlarm(alarmId, QStringLiteral("QML 闭环确认"));
    QTRY_VERIFY_WITH_TIMEOUT(!facade.commandBusy(), 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(
        facade.activeAlarmModel()
            ->data(facade.activeAlarmModel()->index(temperatureRow),
                   QmlAlarmModel::StateRole)
            .toInt(),
        static_cast<int>(AlarmState::ActiveAcknowledged), 2'000);

    QVERIFY(server.setHighTemperatureEnabled(false));
    QTRY_VERIFY_WITH_TIMEOUT(
        alarmRowForTag(facade.activeAlarmModel(),
                       QStringLiteral("temperature")) < 0,
        5'000);

    const int pointsBeforeOutage = facade.trendModel()->rowCount();
    server.stop();
    QTRY_VERIFY_WITH_TIMEOUT(
        alarmRowForTag(facade.activeAlarmModel(), QStringLiteral("通信")) >= 0,
        6'000);
    QVERIFY(server.start(QHostAddress::LocalHost, port, 1));
    QTRY_COMPARE_WITH_TIMEOUT(
        facade.deviceModel()->connectionState(),
        static_cast<int>(ConnectionState::Online), 6'000);
    QTRY_VERIFY_WITH_TIMEOUT(facade.trendModel()->rowCount() > pointsBeforeOutage,
                             4'000);

    facade.disconnectDevice();
    QCOMPARE(facade.deviceModel()->connectionState(),
             static_cast<int>(ConnectionState::Stopped));
    QVERIFY(controller.shutdown(3'000));
    QVERIFY(!controller.isRunning());
    QCOMPARE(fatalSpy.size(), 0);
}

QTEST_GUILESS_MAIN(QmlBackendClosedLoopTest)

#include "tst_qml_backend_closed_loop.moc"
