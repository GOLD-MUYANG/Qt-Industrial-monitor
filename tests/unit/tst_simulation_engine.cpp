#include <QtTest>

#include "RegisterBank.h"
#include "SimulationEngine.h"

class SimulationEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void movesSpeedGraduallyTowardTarget();
    void producesRepeatableEngineeringValues();
    void supportsRepeatableHighTemperatureOverride();
};

void SimulationEngineTest::movesSpeedGraduallyTowardTarget()
{
    RegisterBank bank;
    SimulationEngine engine;
    bank.setTargetSpeed(1700);

    engine.advance(bank);

    QCOMPARE(bank.value(RegisterBank::Speed), quint16(1550));
    QVERIFY(bank.value(RegisterBank::Speed) < bank.value(RegisterBank::TargetSpeed));
}

void SimulationEngineTest::supportsRepeatableHighTemperatureOverride()
{
    RegisterBank bank;
    SimulationEngine engine;

    QVERIFY(engine.setTemperatureOverride(863));
    for (int step = 0; step < 5; ++step) {
        engine.advance(bank);
        QCOMPARE(bank.value(RegisterBank::Temperature), quint16(863));
    }

    engine.clearTemperatureOverride();
    engine.advance(bank);
    QVERIFY(bank.value(RegisterBank::Temperature) >= 415);
    QVERIFY(bank.value(RegisterBank::Temperature) <= 425);
    QVERIFY(!engine.setTemperatureOverride(2'001));
}

void SimulationEngineTest::producesRepeatableEngineeringValues()
{
    RegisterBank first;
    RegisterBank second;
    SimulationEngine firstEngine;
    SimulationEngine secondEngine;

    for (int step = 0; step < 8; ++step) {
        firstEngine.advance(first);
        secondEngine.advance(second);
    }

    QCOMPARE(first.holdingRegisters(), second.holdingRegisters());
    QVERIFY(first.value(RegisterBank::Temperature) >= 415);
    QVERIFY(first.value(RegisterBank::Temperature) <= 425);
    QVERIFY(first.value(RegisterBank::Pressure) >= 118);
    QVERIFY(first.value(RegisterBank::Pressure) <= 122);
    QVERIFY(first.value(RegisterBank::Voltage) >= 2198);
    QVERIFY(first.value(RegisterBank::Voltage) <= 2202);
}

QTEST_GUILESS_MAIN(SimulationEngineTest)

#include "tst_simulation_engine.moc"
