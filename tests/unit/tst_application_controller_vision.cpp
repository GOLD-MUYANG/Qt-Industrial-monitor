#include <QtTest>

#include "ApplicationController.h"

#include <QTemporaryDir>

class ApplicationControllerVisionTest final : public QObject
{
    Q_OBJECT

private slots:
    void ignoresVisionCommandsBeforeStart();
    void isolatesVisionErrorsAndStopsVisionThreadOnShutdown();
};

void ApplicationControllerVisionTest::ignoresVisionCommandsBeforeStart()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ApplicationController controller(
        QStringLiteral(MODBUS_PLUGIN_DIR),
        temporaryDirectory.filePath(QStringLiteral("not-started.db")));
    QSignalSpy errorSpy(&controller, &ApplicationController::visionError);

    controller.openVisionVideo(QStringLiteral("/missing/before-start.avi"));
    controller.playVisionVideo();
    controller.pauseVisionVideo();
    controller.stopVisionVideo();

    QCOMPARE(errorSpy.count(), 0);
    QVERIFY(!controller.isRunning());
}

void ApplicationControllerVisionTest::isolatesVisionErrorsAndStopsVisionThreadOnShutdown()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ApplicationController controller(
        QStringLiteral(MODBUS_PLUGIN_DIR),
        temporaryDirectory.filePath(QStringLiteral("vision-controller.db")));
    QSignalSpy visionErrorSpy(&controller, &ApplicationController::visionError);
    QSignalSpy snapshotSpy(&controller, &ApplicationController::snapshotsReady);
    QSignalSpy alarmSpy(&controller, &ApplicationController::alarmChanged);
    QSignalSpy fatalSpy(&controller, &ApplicationController::fatalError);

    QVERIFY(controller.start());
    QVERIFY(controller.isRunning());
    controller.openVisionVideo(QStringLiteral("/definitely/missing/controller-video.avi"));
    QTRY_COMPARE_WITH_TIMEOUT(visionErrorSpy.count(), 1, 1'000);
    QVERIFY(controller.isRunning());
    QCOMPARE(snapshotSpy.count(), 0);
    QCOMPARE(alarmSpy.count(), 0);
    QCOMPARE(fatalSpy.count(), 0);

    QVERIFY(controller.shutdown(2'000));
    QVERIFY(!controller.isRunning());
}

QTEST_GUILESS_MAIN(ApplicationControllerVisionTest)

#include "tst_application_controller_vision.moc"
