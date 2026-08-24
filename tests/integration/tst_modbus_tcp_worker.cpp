#include <QtTest>

#include <QHostAddress>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTcpServer>

#include <industrial/protocol/AbstractDeviceWorker.h>
#include <industrial/protocol/ProtocolTypes.h>

#include "PluginManager.h"
#include "VirtualPlcServer.h"

using namespace industrial::protocol;

namespace {

bool hasSkippedTransaction(const QSignalSpy &spy)
{
    for (const auto &arguments : spy) {
        if (qvariant_cast<TransactionLog>(arguments.at(0)).skipped) {
            return true;
        }
    }
    return false;
}

quint16 availablePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    return probe.serverPort();
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

bool hasState(const QSignalSpy &spy, ConnectionState expected)
{
    for (const auto &arguments : spy) {
        if (qvariant_cast<DeviceState>(arguments.at(0)).connectionState == expected) {
            return true;
        }
    }
    return false;
}

AbstractDeviceWorker *createWorker(const DeviceConfig &config,
                                   PluginManager &manager)
{
    manager.scan(QString::fromUtf8(MODBUS_PLUGIN_DIR));
    auto *plugin = manager.plugin(QStringLiteral("modbus-tcp"));
    return plugin ? plugin->createWorker(config) : nullptr;
}

} // namespace

class ModbusTcpWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void recordsSkippedPollWhileReadIsInFlight();
    void reconnectsAfterVirtualPlcReturns();
};

void ModbusTcpWorkerTest::initTestCase()
{
    registerProtocolMetaTypes();
}

void ModbusTcpWorkerTest::recordsSkippedPollWhileReadIsInFlight()
{
    QTcpServer unresponsiveServer;
    QVERIFY(unresponsiveServer.listen(QHostAddress::LocalHost, 0));

    DeviceConfig config;
    config.id = QStringLiteral("unresponsive-plc");
    config.port = unresponsiveServer.serverPort();
    config.pollIntervalMs = 20;
    config.timeoutMs = 100;
    config.protocolRetries = 0;
    config.consecutiveFailureLimit = 1;
    config.reconnectDelaysMs = {20};

    PluginManager manager;
    QScopedPointer<AbstractDeviceWorker> worker(createWorker(config, manager));
    QVERIFY(worker);
    QSignalSpy transactionSpy(worker.data(), &AbstractDeviceWorker::transactionLogged);
    QSignalSpy stoppedSpy(worker.data(), &AbstractDeviceWorker::stopped);

    worker->start();
    QTRY_VERIFY_WITH_TIMEOUT(hasSkippedTransaction(transactionSpy), 1'000);

    worker->stop();
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 1'000);
}

void ModbusTcpWorkerTest::reconnectsAfterVirtualPlcReturns()
{
    const quint16 port = availablePort();
    QVERIFY(port != 0);
    VirtualPlcServer server;
    QVERIFY(server.start(QHostAddress::LocalHost, port, 1));

    DeviceConfig config;
    config.id = QStringLiteral("reconnect-plc");
    config.port = port;
    config.pollIntervalMs = 30;
    config.timeoutMs = 100;
    config.protocolRetries = 0;
    config.consecutiveFailureLimit = 1;
    config.reconnectDelaysMs = {20, 40, 80};

    PluginManager manager;
    QScopedPointer<AbstractDeviceWorker> worker(createWorker(config, manager));
    QVERIFY(worker);
    QSignalSpy stateSpy(worker.data(), &AbstractDeviceWorker::stateChanged);
    QSignalSpy samplesSpy(worker.data(), &AbstractDeviceWorker::samplesReady);
    QSignalSpy stoppedSpy(worker.data(), &AbstractDeviceWorker::stopped);

    worker->start();
    QTRY_VERIFY_WITH_TIMEOUT(samplesSpy.count() >= 2, 2'000);
    const int sampleCountBeforeOutage = samplesSpy.count();
    server.stop();
    QTRY_VERIFY_WITH_TIMEOUT(hasState(stateSpy, ConnectionState::Reconnecting), 2'000);
    QVERIFY(server.start(QHostAddress::LocalHost, port, 1));

    QTRY_VERIFY_WITH_TIMEOUT(onlineStateCount(stateSpy) >= 2, 3'000);
    QTRY_VERIFY_WITH_TIMEOUT(samplesSpy.count() > sampleCountBeforeOutage, 3'000);

    worker->stop();
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 1'000);
}

QTEST_GUILESS_MAIN(ModbusTcpWorkerTest)

#include "tst_modbus_tcp_worker.moc"
