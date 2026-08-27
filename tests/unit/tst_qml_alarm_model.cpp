#include "QmlAlarmModel.h"

#include <QtTest>

using namespace industrial::monitor;

namespace {

AlarmRecord alarm(AlarmState state)
{
    AlarmRecord record;
    record.id = QStringLiteral("alarm-1");
    record.deviceId = QStringLiteral("device-1");
    record.tagId = QStringLiteral("temperature");
    record.kind = AlarmKind::Threshold;
    record.severity = AlarmSeverity::Critical;
    record.state = state;
    record.message = QStringLiteral("温度过高");
    record.triggerValue = 86.3;
    record.activatedAtUtc =
        QDateTime::fromString(QStringLiteral("2026-08-27T08:00:00Z"), Qt::ISODate);
    return record;
}

} // namespace

class QmlAlarmModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesRolesAndFiltersActiveRecords();
    void usesStableIdForUpsertAndAcknowledgement();
};

void QmlAlarmModelTest::exposesRolesAndFiltersActiveRecords()
{
    QmlAlarmModel active(QmlAlarmModel::Mode::Active);
    QmlAlarmModel history(QmlAlarmModel::Mode::History);
    const auto activeAlarm = alarm(AlarmState::ActiveUnacknowledged);

    active.upsertAlarm(activeAlarm);
    history.upsertAlarm(activeAlarm);

    QCOMPARE(active.rowCount(), 1);
    QCOMPARE(history.rowCount(), 1);
    const auto roles = active.roleNames();
    QCOMPARE(roles.value(QmlAlarmModel::AlarmIdRole), QByteArray("alarmId"));
    QCOMPARE(roles.value(QmlAlarmModel::AcknowledgeableRole),
             QByteArray("acknowledgeable"));
    QCOMPARE(active.data(active.index(0), QmlAlarmModel::AlarmIdRole).toString(),
             activeAlarm.id);
    QCOMPARE(active.data(active.index(0), QmlAlarmModel::SeverityTextRole).toString(),
             QStringLiteral("严重 (Critical)"));
    QCOMPARE(active.data(active.index(0), QmlAlarmModel::ActiveRole).toBool(), true);
    QCOMPARE(active.data(active.index(0), QmlAlarmModel::AcknowledgeableRole).toBool(), true);
    QVERIFY(history.isAcknowledgeableId(activeAlarm.id));
}

void QmlAlarmModelTest::usesStableIdForUpsertAndAcknowledgement()
{
    QmlAlarmModel active(QmlAlarmModel::Mode::Active);
    QmlAlarmModel history(QmlAlarmModel::Mode::History);
    active.upsertAlarm(alarm(AlarmState::ActiveUnacknowledged));
    history.upsertAlarm(alarm(AlarmState::ActiveUnacknowledged));

    const auto recovered = alarm(AlarmState::RecoveredUnacknowledged);
    active.upsertAlarm(recovered);
    history.upsertAlarm(recovered);

    QCOMPARE(active.rowCount(), 0);
    QCOMPARE(history.rowCount(), 1);
    QCOMPARE(history.alarmIdAt(0), recovered.id);
    QCOMPARE(history.data(history.index(0), QmlAlarmModel::StateTextRole).toString(),
             QStringLiteral("已恢复 / 未确认"));
    QCOMPARE(history.data(history.index(0), QmlAlarmModel::AcknowledgeableRole).toBool(),
             true);

    history.upsertAlarm(alarm(AlarmState::RecoveredAcknowledged));
    QCOMPARE(history.rowCount(), 1);
    QCOMPARE(history.data(history.index(0), QmlAlarmModel::AcknowledgeableRole).toBool(),
             false);
    QVERIFY(!history.isAcknowledgeableId(recovered.id));
}

QTEST_GUILESS_MAIN(QmlAlarmModelTest)

#include "tst_qml_alarm_model.moc"
