#include <QtTest>

#include <QHostAddress>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QThread>
#include <QTcpServer>

#include <industrial/protocol/AbstractDeviceWorker.h>
#include <industrial/protocol/ProtocolTypes.h>

#include "PluginManager.h"
#include "DataPipeline.h"
#include "DeviceSession.h"
#include "VirtualPlcServer.h"

using namespace industrial::protocol;

namespace {

quint16 availablePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    return probe.serverPort();
}

int stateCount(const QSignalSpy &spy, ConnectionState expected)
{
    int count = 0;
    for (const auto &arguments : spy) {
        if (qvariant_cast<DeviceState>(arguments.at(0)).connectionState == expected) {
            ++count;
        }
    }
    return count;
}

int onlineStateCount(const QSignalSpy &spy)
{
    int count = 0;
    for (const auto &arguments : spy) {
        if (qvariant_cast<DeviceState>(arguments.at(0)).connectionState
            == ConnectionState::Online) {
            ++count;
        }
    }
    return count;
}

bool hasSnapshotQuality(const QSignalSpy &spy, DataQuality quality)
{
    for (const auto &arguments : spy) {
        const auto batch = qvariant_cast<RealtimeSnapshotBatch>(arguments.at(0));
        for (const auto &snapshot : batch) {
            if (snapshot.quality == quality) {
                return true;
            }
        }
    }
    return false;
}

int snapshotQualityCount(const QSignalSpy &spy, DataQuality quality)
{
    int count = 0;
    for (const auto &arguments : spy) {
        const auto batch = qvariant_cast<RealtimeSnapshotBatch>(arguments.at(0));
        for (const auto &snapshot : batch) {
            if (snapshot.quality == quality) {
                ++count;
            }
        }
    }
    return count;
}

class ThreadStopper final
{
public:
    explicit ThreadStopper(QThread *thread)
        : m_thread(thread)
    {
    }

    ~ThreadStopper()
    {
        m_thread->quit();
        m_thread->wait(1'000);
    }

private:
    QThread *m_thread;
};

} // namespace

class ModbusPluginIntegrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsFiveRegistersAndWritesTargetSpeed();
    void reconnectsThroughDeviceSessionAndDataPipeline();
};

void ModbusPluginIntegrationTest::initTestCase()
{
    registerProtocolMetaTypes();
}

void ModbusPluginIntegrationTest::readsFiveRegistersAndWritesTargetSpeed()
{
    const quint16 port = availablePort();
    QVERIFY(port != 0);

    VirtualPlcServer server;
    QVERIFY2(server.start(QHostAddress::LocalHost, port, 1),
             qPrintable(server.errorString()));

    PluginManager manager;
    manager.scan(QString::fromUtf8(MODBUS_PLUGIN_DIR));
    QCOMPARE(manager.errors().size(), 0);
    QCOMPARE(manager.plugins().size(), 1);

    auto *plugin = manager.plugin(QStringLiteral("modbus-tcp"));
    QVERIFY(plugin != nullptr);
    QCOMPARE(plugin->descriptor().apiVersion, 1);

    DeviceConfig config;
    config.id = QStringLiteral("plc-1");
    config.host = QStringLiteral("127.0.0.1");
    config.port = port;
    config.unitId = 1;
    config.timeoutMs = 800;
    config.protocolRetries = 1;

    QScopedPointer<AbstractDeviceWorker> worker(plugin->createWorker(config));
    QVERIFY(worker != nullptr);
    QSignalSpy samplesSpy(worker.data(), &AbstractDeviceWorker::samplesReady);
    QSignalSpy writeSpy(worker.data(), &AbstractDeviceWorker::writeFinished);
    QSignalSpy stoppedSpy(worker.data(), &AbstractDeviceWorker::stopped);

    worker->start();
    QTRY_COMPARE_WITH_TIMEOUT(samplesSpy.count(), 1, 3'000);
    const auto samples = qvariant_cast<SampleBatch>(samplesSpy.takeFirst().at(0));
    QCOMPARE(samples.size(), 5);
    QCOMPARE(samples.at(0).tagId, QStringLiteral("temperature"));
    QCOMPARE(samples.at(0).engineeringValue, 42.0);

    WriteRequest request;
    request.deviceId = config.id;
    request.tagId = QStringLiteral("target-speed");
    request.address = RegisterBank::TargetSpeed;
    request.rawValue = 1700;
    request.requestId = 9;
    worker->writeValue(request);

    QTRY_COMPARE_WITH_TIMEOUT(writeSpy.count(), 1, 3'000);
    const auto writeResult =
        qvariant_cast<WriteResult>(writeSpy.takeFirst().at(0));
    QVERIFY2(writeResult.success, qPrintable(writeResult.errorMessage));
    QCOMPARE(writeResult.requestId, quint64(9));
    QTRY_COMPARE_WITH_TIMEOUT(
        server.registerBank().value(RegisterBank::TargetSpeed),
        quint16(1700),
        1'000);

    worker->stop();
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 1'000);
}

void ModbusPluginIntegrationTest::reconnectsThroughDeviceSessionAndDataPipeline()
{
    const quint16 port = availablePort();
    QVERIFY(port != 0);

    VirtualPlcServer server;
    QVERIFY2(server.start(QHostAddress::LocalHost, port, 1),
             qPrintable(server.errorString()));

    PluginManager manager;
    manager.scan(QString::fromUtf8(MODBUS_PLUGIN_DIR));
    auto *plugin = manager.plugin(QStringLiteral("modbus-tcp"));
    QVERIFY(plugin != nullptr);

    DeviceConfig config;
    config.id = QStringLiteral("threaded-plc");
    config.port = port;
    config.pollIntervalMs = 30;
    config.timeoutMs = 100;
    config.protocolRetries = 0;
    config.consecutiveFailureLimit = 1;
    config.reconnectDelaysMs = {20, 40, 80};

    DeviceSession session(plugin, config);
    QSignalSpy stateSpy(&session, &DeviceSession::stateChanged);
    QSignalSpy sampleSpy(&session, &DeviceSession::samplesReady);

    QThread dataThread;
    ThreadStopper dataThreadStopper(&dataThread);
    auto *pipeline = new DataPipeline;
    pipeline->moveToThread(&dataThread);
    connect(&dataThread, &QThread::finished,
            pipeline, &QObject::deleteLater);
    connect(&session, &DeviceSession::samplesReady,
            pipeline, &DataPipeline::processSamples,
            Qt::QueuedConnection);
    connect(&session, &DeviceSession::stateChanged,
            pipeline, &DataPipeline::handleDeviceState,
            Qt::QueuedConnection);
    QSignalSpy snapshotSpy(pipeline, &DataPipeline::snapshotsReady);
    dataThread.start();

    QVERIFY(session.start());
    QTRY_VERIFY_WITH_TIMEOUT(sampleSpy.count() >= 2, 2'000);
    QTRY_VERIFY_WITH_TIMEOUT(hasSnapshotQuality(snapshotSpy, DataQuality::Good), 1'000);
    for (int cycle = 1; cycle <= 2; ++cycle) {
        const int samplesBeforeOutage = sampleSpy.count();
        const int staleBeforeOutage =
            snapshotQualityCount(snapshotSpy, DataQuality::Stale);

        server.stop();
        QTRY_VERIFY_WITH_TIMEOUT(
            stateCount(stateSpy, ConnectionState::Reconnecting) >= cycle,
            2'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            snapshotQualityCount(snapshotSpy, DataQuality::Stale)
                > staleBeforeOutage,
            1'000);
        QVERIFY2(server.start(QHostAddress::LocalHost, port, 1),
                 qPrintable(server.errorString()));

        QTRY_VERIFY_WITH_TIMEOUT(onlineStateCount(stateSpy) >= cycle + 1, 3'000);
        QTRY_VERIFY_WITH_TIMEOUT(sampleSpy.count() > samplesBeforeOutage, 3'000);
    }
    QVERIFY(session.stopAndWait(1'000));
    QVERIFY(!session.isRunning());
}

QTEST_GUILESS_MAIN(ModbusPluginIntegrationTest)

#include "tst_modbus_plugin_integration.moc"
