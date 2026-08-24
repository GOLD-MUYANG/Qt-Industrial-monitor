#include <QtTest>

#include "RegisterBank.h"

class RegisterBankTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesDocumentedDefaultMap();
    void writesOnlyKnownRegisters();
};

void RegisterBankTest::exposesDocumentedDefaultMap()
{
    const RegisterBank bank;

    QCOMPARE(bank.value(RegisterBank::Temperature), quint16(420));
    QCOMPARE(bank.value(RegisterBank::Pressure), quint16(120));
    QCOMPARE(bank.value(RegisterBank::Speed), quint16(1500));
    QCOMPARE(bank.value(RegisterBank::Voltage), quint16(2200));
    QCOMPARE(bank.value(RegisterBank::Status), quint16(1));
    QCOMPARE(bank.value(RegisterBank::TargetSpeed), quint16(1500));
    QCOMPARE(bank.holdingRegisters().size(), int(RegisterBank::HoldingRegisterCount));
}

void RegisterBankTest::writesOnlyKnownRegisters()
{
    RegisterBank bank;

    QVERIFY(bank.setTargetSpeed(2400));
    QCOMPARE(bank.value(RegisterBank::TargetSpeed), quint16(2400));
    QVERIFY(!bank.setValue(5, 99));
}

QTEST_GUILESS_MAIN(RegisterBankTest)

#include "tst_register_bank.moc"
