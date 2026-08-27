#include <QtTest>

#include "AlarmTableModel.h"
#include "DeviceTableModel.h"
#include "RealtimeTableModel.h"

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

RealtimeSnapshot snapshot(const QString &tagId,
                          double value,
                          DataQuality quality = DataQuality::Good)
{
    RealtimeSnapshot result;
    result.deviceId = QStringLiteral("virtual-plc-1");
    result.tagId = tagId;
    result.current = value;
    result.minimum = value - 1.0;
    result.maximum = value + 1.0;
    result.average = value - 0.25;
    result.sampleCount = 3;
    result.quality = quality;
    result.timestampUtc =
        QDateTime::fromMSecsSinceEpoch(1'700'000'000'000LL, Qt::UTC);
    result.sequence = 3;
    return result;
}

AlarmRecord alarm(AlarmState state)
{
    AlarmRecord result;
    result.id = QStringLiteral("alarm-1");
    result.ruleId = QStringLiteral("temperature-range");
    result.deviceId = QStringLiteral("virtual-plc-1");
    result.tagId = QStringLiteral("temperature");
    result.kind = AlarmKind::Threshold;
    result.severity = AlarmSeverity::Critical;
    result.state = state;
    result.message = QStringLiteral("温度超出 10..80 ℃");
    result.triggerValue = 86.3;
    result.activatedAtUtc =
        QDateTime::fromMSecsSinceEpoch(1'700'000'000'000LL, Qt::UTC);
    if (state == AlarmState::RecoveredUnacknowledged
        || state == AlarmState::RecoveredAcknowledged) {
        result.recoveredAtUtc = result.activatedAtUtc.addSecs(5);
    }
    return result;
}

} // namespace

class PresentationModelsTest final : public QObject
{
    Q_OBJECT

private slots:
    void realtimeModelShowsValueQualityAndStatisticsAsText();
    void deviceModelShowsEndpointAndConnectionState();
    void alarmModelsSeparateActiveRowsAndKeepHistory();
};

void PresentationModelsTest::realtimeModelShowsValueQualityAndStatisticsAsText()
{
    RealtimeTableModel model;
    model.applySnapshots({snapshot(QStringLiteral("temperature"), 42.5)});
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.headerData(0, Qt::Horizontal).toString(),
             QStringLiteral("测点"));
    QCOMPARE(model.data(model.index(0, RealtimeTableModel::CurrentColumn)).toString(),
             QStringLiteral("42.50"));
    QVERIFY(model.data(model.index(0, RealtimeTableModel::QualityColumn))
                .toString()
                .contains(QStringLiteral("Good")));
    QVERIFY(model.data(model.index(0, RealtimeTableModel::QualityColumn),
                       Qt::ForegroundRole)
                .isValid());

    model.applySnapshots({snapshot(QStringLiteral("temperature"), 42.5,
                                   DataQuality::Stale)});
    QVERIFY(model.data(model.index(0, RealtimeTableModel::QualityColumn))
                .toString()
                .contains(QStringLiteral("Stale")));
}

void PresentationModelsTest::deviceModelShowsEndpointAndConnectionState()
{
    DeviceTableModel model;
    DeviceConfig config;
    config.id = QStringLiteral("virtual-plc-1");
    config.protocolKey = QStringLiteral("modbus-tcp");
    config.host = QStringLiteral("127.0.0.1");
    config.port = 1502;
    model.addProtocol({QStringLiteral("modbus-tcp"),
                       QStringLiteral("Modbus TCP"), 1, {}});
    model.setDevice(config);

    DeviceState state;
    state.deviceId = config.id;
    state.connectionState = ConnectionState::Reconnecting;
    model.setState(state);
    model.setLastCommunicationTime(
        QDateTime::fromMSecsSinceEpoch(1'700'000'000'000LL, Qt::UTC));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, DeviceTableModel::EndpointColumn)).toString(),
             QStringLiteral("127.0.0.1:1502"));
    QCOMPARE(model.data(model.index(0, DeviceTableModel::ProtocolColumn)).toString(),
             QStringLiteral("Modbus TCP"));
    QVERIFY(!model.data(model.index(0, DeviceTableModel::LastCommunicationColumn))
                 .toString().isEmpty());
    QVERIFY(model.data(model.index(0, DeviceTableModel::StateColumn))
                .toString()
                .contains(QStringLiteral("Reconnecting")));
}

void PresentationModelsTest::alarmModelsSeparateActiveRowsAndKeepHistory()
{
    AlarmTableModel activeModel(AlarmTableModel::Mode::Active);
    AlarmTableModel historyModel(AlarmTableModel::Mode::History);
    auto record = alarm(AlarmState::ActiveUnacknowledged);
    activeModel.upsertAlarm(record);
    historyModel.upsertAlarm(record);

    QCOMPARE(activeModel.rowCount(), 1);
    QCOMPARE(historyModel.rowCount(), 1);
    QVERIFY(activeModel.data(activeModel.index(0, AlarmTableModel::SeverityColumn))
                .toString()
                .contains(QStringLiteral("Critical")));
    QVERIFY(activeModel.data(activeModel.index(0, AlarmTableModel::StateColumn))
                .toString()
                .contains(QStringLiteral("未确认")));

    record.state = AlarmState::RecoveredUnacknowledged;
    record.recoveredAtUtc = record.activatedAtUtc.addSecs(5);
    activeModel.upsertAlarm(record);
    historyModel.upsertAlarm(record);
    QCOMPARE(activeModel.rowCount(), 0);
    QCOMPARE(historyModel.rowCount(), 1);
    QVERIFY(historyModel.data(historyModel.index(0, AlarmTableModel::StateColumn))
                .toString()
                .contains(QStringLiteral("已恢复")));
    QCOMPARE(historyModel.alarmIdAt(0), QStringLiteral("alarm-1"));
}

QTEST_GUILESS_MAIN(PresentationModelsTest)

#include "tst_presentation_models.moc"
