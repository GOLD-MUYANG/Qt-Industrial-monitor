#include "ApplicationController.h"
#include "MainWindow.h"
#include "UiFont.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IndustrialMonitor"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QApplication::setOrganizationName(QStringLiteral("Muyang"));
    configureChineseUiFont();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("第三周报警、SQLite 与主要界面验收入口"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("plugin-dir"), QStringLiteral("动态协议插件目录"),
                      QStringLiteral("directory")});
    parser.addOption({QStringLiteral("database"),
                      QStringLiteral("SQLite 数据库文件；默认写入应用数据目录"),
                      QStringLiteral("file")});
    parser.process(application);

    const QString pluginDirectory =
        parser.isSet(QStringLiteral("plugin-dir"))
            ? QDir::cleanPath(parser.value(QStringLiteral("plugin-dir")))
            : QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));

    QString databasePath = parser.value(QStringLiteral("database"));
    if (databasePath.isEmpty())
    {
        const QString dataDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDirectory);
        databasePath = QDir(dataDirectory).filePath(QStringLiteral("industrial_monitor.db"));
    }

    // Controller负责具体的信息交互，可以认为是一个中间层
    // window只进行具体互动
    MainWindow window;
    ApplicationController controller(pluginDirectory, databasePath);

    // 控制器只暴露值对象和命令，MainWindow 不接触协议或 SQL Worker。
    QObject::connect(&controller, &ApplicationController::protocolAvailable, &window,
                     &MainWindow::addProtocol);
    QObject::connect(&controller, &ApplicationController::snapshotsReady, &window,
                     &MainWindow::applySnapshots);
    QObject::connect(&controller, &ApplicationController::alarmChanged, &window,
                     &MainWindow::upsertAlarm);
    QObject::connect(&controller, &ApplicationController::deviceConfigChanged, &window,
                     &MainWindow::setDevice);
    QObject::connect(&controller, &ApplicationController::deviceStateChanged, &window,
                     &MainWindow::setDeviceState);
    QObject::connect(&controller, &ApplicationController::writeFinished, &window,
                     &MainWindow::showWriteResult);
    QObject::connect(&controller, &ApplicationController::storageStatusChanged, &window,
                     &MainWindow::setStorageStatus);
    QObject::connect(&controller, &ApplicationController::fatalError, &window,
                     [&window](const QString &message)
                     { window.setStorageStatus(message, false); });

    QObject::connect(&window, &MainWindow::connectRequested, &controller,
                     &ApplicationController::connectDevice);
    QObject::connect(&window, &MainWindow::disconnectRequested, &controller,
                     &ApplicationController::disconnectDevice);
    QObject::connect(&window, &MainWindow::writeTargetSpeedRequested, &controller,
                     &ApplicationController::writeTargetSpeed);
    QObject::connect(&window, &MainWindow::saveDeviceRequested, &controller,
                     &ApplicationController::applyDeviceConfig);
    QObject::connect(&window, &MainWindow::acknowledgeRequested, &controller,
                     &ApplicationController::acknowledgeAlarm);
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &controller,
                     [&controller]() { controller.shutdown(); });

    window.show();
    controller.start();
    return application.exec();
}
