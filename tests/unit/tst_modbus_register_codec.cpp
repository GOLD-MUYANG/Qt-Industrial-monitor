#include <QtTest>

#include "ModbusRegisterCodec.h"

using namespace industrial::protocol;

class ModbusRegisterCodecTest final : public QObject
{
    Q_OBJECT

private slots:
    void decodesFiveHoldingRegisters();
    void decodesSignedTemperature();
    void rejectsUnexpectedRegisterCount();
};

void ModbusRegisterCodecTest::decodesFiveHoldingRegisters()
{
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    const auto result = ModbusRegisterCodec::decodeSnapshot(
        {420, 120, 1500, 2200, 1}, QStringLiteral("plc-1"), timestamp, 7);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.samples.size(), 5);
    QCOMPARE(result.samples.at(0).tagId, QStringLiteral("temperature"));
    QCOMPARE(result.samples.at(0).engineeringValue, 42.0);
    QCOMPARE(result.samples.at(1).engineeringValue, 1.2);
    QCOMPARE(result.samples.at(2).engineeringValue, 1500.0);
    QCOMPARE(result.samples.at(3).engineeringValue, 220.0);
    QCOMPARE(result.samples.at(4).engineeringValue, 1.0);
    QCOMPARE(result.samples.at(0).sequence, quint64(7));
    QCOMPARE(result.samples.at(0).timestampUtc, timestamp);
}

void ModbusRegisterCodecTest::decodesSignedTemperature()
{
    const auto result = ModbusRegisterCodec::decodeSnapshot(
        {quint16(0xff9c), 120, 1500, 2200, 1},
        QStringLiteral("plc-1"), QDateTime::currentDateTimeUtc(), 1);

    QVERIFY(result.success);
    QCOMPARE(result.samples.at(0).engineeringValue, -10.0);
}

void ModbusRegisterCodecTest::rejectsUnexpectedRegisterCount()
{
    const auto result = ModbusRegisterCodec::decodeSnapshot(
        {420, 120}, QStringLiteral("plc-1"), QDateTime::currentDateTimeUtc(), 1);

    QVERIFY(!result.success);
    QVERIFY(result.samples.isEmpty());
    QVERIFY(result.errorMessage.contains(QStringLiteral("5")));
}

QTEST_GUILESS_MAIN(ModbusRegisterCodecTest)

#include "tst_modbus_register_codec.moc"
