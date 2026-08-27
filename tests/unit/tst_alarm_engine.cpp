#include <QtTest>

#include "AlarmEngine.h"

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

AlarmRule highTemperatureRule()
{
    AlarmRule rule;
    rule.id = QStringLiteral("temperature-high");
    rule.deviceId = QStringLiteral("virtual-plc-1");
    rule.tagId = QStringLiteral("temperature");
    rule.kind = AlarmKind::Threshold;
    rule.hasUpperLimit = true;
    rule.upperLimit = 80.0;
    rule.hysteresis = 2.0;
    rule.activationSamples = 3;
    rule.recoverySamples = 3;
    rule.severity = AlarmSeverity::Critical;
    rule.message = QStringLiteral("温度高于 80 ℃");
    return rule;
}

AlarmRule communicationRule()
{
    AlarmRule rule;
    rule.id = QStringLiteral("communication-disconnected");
    rule.deviceId = QStringLiteral("virtual-plc-1");
    rule.kind = AlarmKind::Communication;
    rule.severity = AlarmSeverity::Critical;
    rule.message = QStringLiteral("设备通信中断");
    return rule;
}

MeasurementSample temperature(double value, quint64 sequence)
{
    MeasurementSample sample;
    sample.deviceId = QStringLiteral("virtual-plc-1");
    sample.tagId = QStringLiteral("temperature");
    sample.engineeringValue = value;
    sample.quality = DataQuality::Good;
    sample.timestampUtc = QDateTime::fromMSecsSinceEpoch(
        1'700'000'000'000LL + static_cast<qint64>(sequence) * 500,
        Qt::UTC);
    sample.sequence = sequence;
    return sample;
}

AlarmRecord lastAlarm(const QSignalSpy &spy)
{
    return qvariant_cast<AlarmRecord>(spy.constLast().at(0));
}

} // namespace

class AlarmEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void activatesOnlyAfterThreeConsecutiveGoodSamples();
    void recoversOnlyInsideHysteresisForThreeSamples();
    void preservesAcknowledgementAcrossRecovery();
    void ignoresBadAndStaleSamples();
    void createsAndRecoversSingleCommunicationAlarm();
};

void AlarmEngineTest::initTestCase()
{
    registerAlarmMetaTypes();
}

void AlarmEngineTest::activatesOnlyAfterThreeConsecutiveGoodSamples()
{
    AlarmEngine engine;
    engine.setRules({highTemperatureRule()});
    QSignalSpy alarmSpy(&engine, &AlarmEngine::alarmChanged);

    engine.processSamples({temperature(81.0, 1)});
    engine.processSamples({temperature(79.0, 2)});
    engine.processSamples({temperature(82.0, 3)});
    engine.processSamples({temperature(83.0, 4)});
    QCOMPARE(alarmSpy.count(), 0);

    engine.processSamples({temperature(84.0, 5)});
    QCOMPARE(alarmSpy.count(), 1);
    const auto active = lastAlarm(alarmSpy);
    QVERIFY(!active.id.isEmpty());
    QCOMPARE(active.ruleId, QStringLiteral("temperature-high"));
    QCOMPARE(active.state, AlarmState::ActiveUnacknowledged);
    QCOMPARE(active.triggerValue, 84.0);

    engine.processSamples({temperature(85.0, 6)});
    QCOMPARE(alarmSpy.count(), 1);
}

void AlarmEngineTest::recoversOnlyInsideHysteresisForThreeSamples()
{
    AlarmEngine engine;
    engine.setRules({highTemperatureRule()});
    QSignalSpy alarmSpy(&engine, &AlarmEngine::alarmChanged);

    for (quint64 sequence = 1; sequence <= 3; ++sequence) {
        engine.processSamples({temperature(86.3, sequence)});
    }
    const QString alarmId = lastAlarm(alarmSpy).id;

    engine.processSamples({temperature(79.0, 4)});
    engine.processSamples({temperature(77.5, 5)});
    engine.processSamples({temperature(79.0, 6)});
    QCOMPARE(alarmSpy.count(), 1);

    engine.processSamples({temperature(77.0, 7)});
    engine.processSamples({temperature(76.0, 8)});
    engine.processSamples({temperature(75.0, 9)});
    QCOMPARE(alarmSpy.count(), 2);
    const auto recovered = lastAlarm(alarmSpy);
    QCOMPARE(recovered.id, alarmId);
    QCOMPARE(recovered.state, AlarmState::RecoveredUnacknowledged);
    QVERIFY(recovered.recoveredAtUtc.isValid());
}

void AlarmEngineTest::preservesAcknowledgementAcrossRecovery()
{
    AlarmEngine engine;
    engine.setRules({highTemperatureRule()});
    QSignalSpy alarmSpy(&engine, &AlarmEngine::alarmChanged);
    for (quint64 sequence = 1; sequence <= 3; ++sequence) {
        engine.processSamples({temperature(86.3, sequence)});
    }

    const QString alarmId = lastAlarm(alarmSpy).id;
    const QDateTime acknowledgementTime =
        QDateTime::fromMSecsSinceEpoch(1'700'000'010'000LL, Qt::UTC);
    QVERIFY(engine.acknowledge(alarmId, QStringLiteral("值班员确认"),
                               acknowledgementTime));
    QCOMPARE(lastAlarm(alarmSpy).state, AlarmState::ActiveAcknowledged);
    QCOMPARE(lastAlarm(alarmSpy).acknowledgementNote,
             QStringLiteral("值班员确认"));

    for (quint64 sequence = 4; sequence <= 6; ++sequence) {
        engine.processSamples({temperature(77.0, sequence)});
    }
    QCOMPARE(lastAlarm(alarmSpy).id, alarmId);
    QCOMPARE(lastAlarm(alarmSpy).state, AlarmState::RecoveredAcknowledged);
}

void AlarmEngineTest::ignoresBadAndStaleSamples()
{
    AlarmEngine engine;
    engine.setRules({highTemperatureRule()});
    QSignalSpy alarmSpy(&engine, &AlarmEngine::alarmChanged);

    auto bad = temperature(100.0, 1);
    bad.quality = DataQuality::Bad;
    auto stale = temperature(100.0, 2);
    stale.quality = DataQuality::Stale;
    engine.processSamples({bad, stale});
    engine.processSamples({temperature(86.3, 3)});
    engine.processSamples({temperature(86.3, 4)});
    QCOMPARE(alarmSpy.count(), 0);
}

void AlarmEngineTest::createsAndRecoversSingleCommunicationAlarm()
{
    AlarmEngine engine;
    engine.setRules({communicationRule()});
    QSignalSpy alarmSpy(&engine, &AlarmEngine::alarmChanged);

    DeviceState state;
    state.deviceId = QStringLiteral("virtual-plc-1");
    state.connectionState = ConnectionState::Reconnecting;
    state.message = QStringLiteral("连接失败");
    engine.handleDeviceState(state);
    engine.handleDeviceState(state);
    QCOMPARE(alarmSpy.count(), 1);
    const QString alarmId = lastAlarm(alarmSpy).id;
    QCOMPARE(lastAlarm(alarmSpy).kind, AlarmKind::Communication);

    state.connectionState = ConnectionState::Online;
    state.message.clear();
    engine.handleDeviceState(state);
    QCOMPARE(alarmSpy.count(), 2);
    QCOMPARE(lastAlarm(alarmSpy).id, alarmId);
    QCOMPARE(lastAlarm(alarmSpy).state,
             AlarmState::RecoveredUnacknowledged);
}

QTEST_APPLESS_MAIN(AlarmEngineTest)

#include "tst_alarm_engine.moc"
