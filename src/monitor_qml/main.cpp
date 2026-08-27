#include "ApplicationController.h"
#include "QmlApplicationFacade.h"
#include "UiFont.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IndustrialMonitorQml"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QApplication::setOrganizationName(QStringLiteral("Muyang"));
    configureChineseUiFont();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("工业设备监控系统 QML 并行界面"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("plugin-dir"),
                      QStringLiteral("动态协议插件目录"),
                      QStringLiteral("directory")});
    parser.addOption({QStringLiteral("database"),
                      QStringLiteral("SQLite 数据库文件；默认写入应用数据目录"),
                      QStringLiteral("file")});
    parser.process(application);

    const QString pluginDirectory =
        parser.isSet(QStringLiteral("plugin-dir"))
        ? QDir::cleanPath(parser.value(QStringLiteral("plugin-dir")))
        : QDir(QCoreApplication::applicationDirPath())
              .filePath(QStringLiteral("plugins"));

    QString databasePath = parser.value(QStringLiteral("database"));
    if (databasePath.isEmpty())
    {
        const QString dataDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDirectory);
        databasePath = QDir(dataDirectory)
                           .filePath(QStringLiteral("industrial_monitor.db"));
    }

    // 声明顺序保证退出时 Engine 先于 Facade 和 Controller 析构。
    ApplicationController controller(pluginDirectory, databasePath);
    QmlApplicationFacade facade(&controller);
    QQmlApplicationEngine engine;
    // Qt 6.2 默认导入根仍偏向旧路径，显式加入嵌入模块前缀。
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.rootContext()->setContextProperty(QStringLiteral("appFacade"),
                                             &facade);

    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &controller,
                     [&controller]() { controller.shutdown(); });

    const QUrl mainUrl(
        QStringLiteral("qrc:/qt/qml/IndustrialMonitor/Qml/Main.qml"));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty())
    {
        return EXIT_FAILURE;
    }

    // 只有 QML 根对象创建成功后才启动通信、数据和数据库线程。
    controller.start();
    return application.exec();
}
