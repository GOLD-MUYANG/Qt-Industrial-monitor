#include <QtTest>

#include <industrial/protocol/ProtocolTypes.h>

using namespace industrial::protocol;

class ProtocolTypesTest final : public QObject
{
    Q_OBJECT

private slots:
    void registersQueuedConnectionTypes();
    void providesWeekTwoCommunicationDefaults();
};

void ProtocolTypesTest::registersQueuedConnectionTypes()
{
    registerProtocolMetaTypes();

    QVERIFY(QMetaType::fromName("industrial::protocol::DeviceState").isValid());
    QVERIFY(QMetaType::fromName("industrial::protocol::SampleBatch").isValid());
    QVERIFY(QMetaType::fromName("industrial::protocol::WriteRequest").isValid());
    QVERIFY(QMetaType::fromName("industrial::protocol::DeviceErrorCategory").isValid());
    QVERIFY(QMetaType::fromName("industrial::protocol::RealtimeSnapshotBatch").isValid());
}

void ProtocolTypesTest::providesWeekTwoCommunicationDefaults()
{
    const DeviceConfig config;

    QCOMPARE(config.pollIntervalMs, 500);
    QCOMPARE(config.consecutiveFailureLimit, 3);
    QCOMPARE(config.reconnectDelaysMs,
             QList<int>({1'000, 2'000, 4'000, 8'000, 10'000}));

    const DeviceError error;
    QCOMPARE(error.category, DeviceErrorCategory::Connection);
    QVERIFY(error.recoverable);

    const RealtimeSnapshot snapshot;
    QCOMPARE(snapshot.quality, DataQuality::Bad);
    QCOMPARE(snapshot.sampleCount, 0);
}

QTEST_GUILESS_MAIN(ProtocolTypesTest)

#include "tst_protocol_types.moc"
