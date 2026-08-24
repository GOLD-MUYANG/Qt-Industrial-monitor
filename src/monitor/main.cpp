#include "DataPipeline.h"
#include "DeviceSession.h"
#include "PluginManager.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <industrial/protocol/ProtocolTypes.h>

using namespace industrial::protocol;

namespace {

QString stateName(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Stopped:
        return QStringLiteral("Stopped");
    case ConnectionState::Connecting:
        return QStringLiteral("Connecting");
    case ConnectionState::Online:
        return QStringLiteral("Online");
    case ConnectionState::Reconnecting:
        return QStringLiteral("Reconnecting");
    case ConnectionState::Stopping:
        return QStringLiteral("Stopping");
    case ConnectionState::Faulted:
        return QStringLiteral("Faulted");
    }
    return QStringLiteral("Unknown");
}

QString qualityName(DataQuality quality)
{
    switch (quality) {
    case DataQuality::Good:
        return QStringLiteral("Good");
    case DataQuality::Stale:
        return QStringLiteral("Stale");
    case DataQuality::Bad:
        return QStringLiteral("Bad");
    }
    return QStringLiteral("Unknown");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("industrial_monitor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("第二周通信线程、自动重连和实时统计验收入口"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("host"),
                      QStringLiteral("PLC 主机"),
                      QStringLiteral("host"),
                      QStringLiteral("127.0.0.1")});
    parser.addOption({QStringLiteral("port"),
                      QStringLiteral("PLC 端口"),
                      QStringLiteral("port"),
                      QStringLiteral("1502")});
    parser.addOption({QStringLiteral("duration-ms"),
                      QStringLiteral("运行时长，0 表示不自动停止"),
                      QStringLiteral("milliseconds"),
                      QStringLiteral("15000")});
    parser.process(application);

    bool validPort = false;
    const uint parsedPort = parser.value(QStringLiteral("port")).toUInt(&validPort);
    bool validDuration = false;
    const int durationMs =
        parser.value(QStringLiteral("duration-ms")).toInt(&validDuration);
    if (!validPort || parsedPort == 0 || parsedPort > 65'535
        || !validDuration || durationMs < 0) {
        QTextStream(stderr)
            << "端口必须在 1..65535，运行时长必须是非负整数" << Qt::endl;
        return 2;
    }

    registerProtocolMetaTypes();

    // PluginManager 必须比 DeviceSession 活得更久，确保插件代码在线程退出后才卸载。
    PluginManager pluginManager;
    const QString pluginDirectory =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
    pluginManager.scan(pluginDirectory);
    if (!pluginManager.errors().isEmpty()) {
        QTextStream error(stderr);
        for (const auto &item : pluginManager.errors()) {
            error << item.filePath << ": " << item.message << Qt::endl;
        }
        return 2;
    }

    auto *plugin = pluginManager.plugin(QStringLiteral("modbus-tcp"));
    if (!plugin) {
        QTextStream(stderr) << "未发现 modbus-tcp 插件" << Qt::endl;
        return 2;
    }

    DeviceConfig config;
    config.id = QStringLiteral("virtual-plc-1");
    config.host = parser.value(QStringLiteral("host"));
    config.port = static_cast<quint16>(parsedPort);
    DeviceSession session(plugin, config);

    // DataPipeline 独占一个共享数据线程，跨线程链路只传递已注册 DTO。
    QThread dataThread;
    dataThread.setObjectName(QStringLiteral("data-pipeline"));
    auto *pipeline = new DataPipeline;
    pipeline->moveToThread(&dataThread);
    QObject::connect(&dataThread, &QThread::finished,
                     pipeline, &QObject::deleteLater);
    QObject::connect(&session, &DeviceSession::samplesReady,
                     pipeline, &DataPipeline::processSamples,
                     Qt::QueuedConnection);
    QObject::connect(&session, &DeviceSession::stateChanged,
                     pipeline, &DataPipeline::handleDeviceState,
                     Qt::QueuedConnection);

    QObject::connect(&session, &DeviceSession::stateChanged, &application,
                     [](const DeviceState &state) {
                         QTextStream(stdout)
                             << "state=" << stateName(state.connectionState)
                             << (state.message.isEmpty()
                                     ? QString()
                                     : QStringLiteral(" message=%1").arg(state.message))
                             << Qt::endl;
                     });
    QObject::connect(&session, &DeviceSession::communicationError, &application,
                     [](const DeviceError &error) {
                         QTextStream(stderr)
                             << error.deviceId
                             << " category=" << static_cast<int>(error.category)
                             << " code=" << error.code
                             << " request=" << error.requestId
                             << ": " << error.message << Qt::endl;
                     });
    QObject::connect(pipeline, &DataPipeline::pipelineError, &application,
                     [](const DeviceError &error) {
                         QTextStream(stderr)
                             << error.deviceId << " data: " << error.message << Qt::endl;
                     },
                     Qt::QueuedConnection);
    QObject::connect(pipeline, &DataPipeline::snapshotsReady, &application,
                     [](const RealtimeSnapshotBatch &snapshots) {
                         QTextStream output(stdout);
                         for (const auto &snapshot : snapshots) {
                             output << snapshot.tagId
                                    << " current=" << snapshot.current
                                    << " min=" << snapshot.minimum
                                    << " max=" << snapshot.maximum
                                    << " avg=" << snapshot.average
                                    << " quality=" << qualityName(snapshot.quality)
                                    << " samples=" << snapshot.sampleCount
                                    << Qt::endl;
                         }
                     },
                     Qt::QueuedConnection);

    QObject::connect(&session, &DeviceSession::stopped,
                     &dataThread, &QThread::quit);
    QObject::connect(&dataThread, &QThread::finished,
                     &application, &QCoreApplication::quit);

    dataThread.start();
    if (!session.start()) {
        dataThread.quit();
        dataThread.wait(1'000);
        return 2;
    }

    QTimer runTimer;
    runTimer.setSingleShot(true);
    if (durationMs > 0) {
        QObject::connect(&runTimer, &QTimer::timeout,
                         &session, &DeviceSession::requestStop);
        runTimer.start(durationMs);
    }

    const int exitCode = application.exec();
    session.stopAndWait(3'000);
    dataThread.quit();
    dataThread.wait(3'000);
    return exitCode;
}
