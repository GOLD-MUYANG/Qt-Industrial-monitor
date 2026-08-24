#include <QtTest>

#include <QHostAddress>
#include <QTcpServer>

#include "VirtualPlcServer.h"

namespace {

quint16 availablePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    return probe.serverPort();
}

} // namespace

class VirtualPlcServerTest final : public QObject
{
    Q_OBJECT

private slots:
    void startsWithDocumentedRegisterMap();
    void canRestartAfterStop();
};

void VirtualPlcServerTest::startsWithDocumentedRegisterMap()
{
    const quint16 port = availablePort();
    QVERIFY(port != 0);
    VirtualPlcServer server;

    QVERIFY2(server.start(QHostAddress::LocalHost, port, 1),
             qPrintable(server.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(server.isRunning(), 1000);
    QCOMPARE(server.registerBank().value(RegisterBank::Temperature), quint16(420));
    QCOMPARE(server.registerBank().value(RegisterBank::TargetSpeed), quint16(1500));

    server.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!server.isRunning(), 1000);
}

void VirtualPlcServerTest::canRestartAfterStop()
{
    const quint16 firstPort = availablePort();
    const quint16 secondPort = availablePort();
    QVERIFY(firstPort != 0);
    QVERIFY(secondPort != 0);
    VirtualPlcServer server;

    QVERIFY(server.start(QHostAddress::LocalHost, firstPort, 1));
    server.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!server.isRunning(), 1000);
    QVERIFY2(server.start(QHostAddress::LocalHost, secondPort, 1),
             qPrintable(server.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(server.isRunning(), 1000);
}

QTEST_GUILESS_MAIN(VirtualPlcServerTest)

#include "tst_virtual_plc_server.moc"
