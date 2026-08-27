#include "ApplicationController.h"
#include "QmlApplicationFacade.h"
#include "UiFont.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRawFont>
#include <QTemporaryDir>
#include <QtTest>

class QmlSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsApplicationShellAndPrimaryControls();
};

void QmlSmokeTest::loadsApplicationShellAndPrimaryControls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(configureChineseUiFont());
    const QRawFont rawFont = QRawFont::fromFont(QApplication::font());
    QVERIFY(rawFont.isValid());
    QVERIFY(rawFont.supportsCharacter(QChar(u'测')));
    ApplicationController controller(
        {}, directory.filePath(QStringLiteral("qml-smoke.db")));
    QmlApplicationFacade facade(&controller);
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.rootContext()->setContextProperty(QStringLiteral("appFacade"),
                                             &facade);

    engine.load(QUrl(QStringLiteral(
        "qrc:/qt/qml/IndustrialMonitor/Qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QCOMPARE(root->objectName(), QStringLiteral("mainWindow"));
    const QStringList requiredObjects = {
        QStringLiteral("navigationList"),
        QStringLiteral("globalStatusBar"),
        QStringLiteral("realtimePage"),
        QStringLiteral("devicePage"),
        QStringLiteral("alarmPage"),
        QStringLiteral("connectButton"),
        QStringLiteral("disconnectButton"),
        QStringLiteral("targetSpeedField"),
        QStringLiteral("writeTargetSpeedButton"),
        QStringLiteral("deviceHostField"),
        QStringLiteral("saveDeviceButton"),
        QStringLiteral("alarmNoteField"),
        QStringLiteral("acknowledgeAlarmButton"),
    };
    for (const auto &objectName : requiredObjects)
    {
        QVERIFY2(root->findChild<QObject *>(objectName),
                 qPrintable(QStringLiteral("找不到 QML 对象：%1")
                                .arg(objectName)));
    }

    QCOMPARE(root->property("title").toString(),
             QStringLiteral("工业设备监控系统"));
}

QTEST_MAIN(QmlSmokeTest)

#include "tst_qml_smoke.moc"
