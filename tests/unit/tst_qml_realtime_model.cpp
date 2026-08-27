#include "QmlRealtimeModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace industrial::protocol;

class QmlRealtimeModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesStableNamedRoles();
    void updatesOnlyTheMatchingTagWithNumericValues();
    void ignoresUnknownTags();
};

void QmlRealtimeModelTest::exposesStableNamedRoles()
{
    QmlRealtimeModel model;

    QCOMPARE(model.rowCount(), 5);
    const auto roles = model.roleNames();
    QCOMPARE(roles.value(QmlRealtimeModel::TagIdRole), QByteArray("tagId"));
    QCOMPARE(roles.value(QmlRealtimeModel::DisplayNameRole), QByteArray("displayName"));
    QCOMPARE(roles.value(QmlRealtimeModel::UnitRole), QByteArray("unit"));
    QCOMPARE(roles.value(QmlRealtimeModel::CurrentValueRole), QByteArray("currentValue"));
    QCOMPARE(roles.value(QmlRealtimeModel::MinimumValueRole), QByteArray("minimumValue"));
    QCOMPARE(roles.value(QmlRealtimeModel::MaximumValueRole), QByteArray("maximumValue"));
    QCOMPARE(roles.value(QmlRealtimeModel::AverageValueRole), QByteArray("averageValue"));
    QCOMPARE(roles.value(QmlRealtimeModel::QualityRole), QByteArray("quality"));
    QCOMPARE(roles.value(QmlRealtimeModel::QualityTextRole), QByteArray("qualityText"));
    QCOMPARE(roles.value(QmlRealtimeModel::TimestampTextRole), QByteArray("timestampText"));
    QCOMPARE(model.data(model.index(0), QmlRealtimeModel::TagIdRole).toString(),
             QStringLiteral("temperature"));
    QCOMPARE(model.data(model.index(0), QmlRealtimeModel::DisplayNameRole).toString(),
             QStringLiteral("温度"));
    QCOMPARE(model.data(model.index(0), QmlRealtimeModel::UnitRole).toString(),
             QStringLiteral("℃"));
}

void QmlRealtimeModelTest::updatesOnlyTheMatchingTagWithNumericValues()
{
    QmlRealtimeModel model;
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    const QDateTime timestamp =
        QDateTime::fromString(QStringLiteral("2026-08-27T10:20:30.000Z"), Qt::ISODateWithMs);

    RealtimeSnapshot snapshot;
    snapshot.deviceId = QStringLiteral("device-1");
    snapshot.tagId = QStringLiteral("speed");
    snapshot.current = 1450.0;
    snapshot.minimum = 1400.0;
    snapshot.maximum = 1500.0;
    snapshot.average = 1442.5;
    snapshot.quality = DataQuality::Good;
    snapshot.timestampUtc = timestamp;
    model.applySnapshots({snapshot});

    QCOMPARE(changedSpy.size(), 1);
    const auto arguments = changedSpy.takeFirst();
    QCOMPARE(qvariant_cast<QModelIndex>(arguments.at(0)).row(), 2);
    QCOMPARE(qvariant_cast<QModelIndex>(arguments.at(1)).row(), 2);
    const QModelIndex speed = model.index(2);
    QCOMPARE(model.data(speed, QmlRealtimeModel::CurrentValueRole).metaType().id(),
             QMetaType::Double);
    QCOMPARE(model.data(speed, QmlRealtimeModel::CurrentValueRole).toDouble(), 1450.0);
    QCOMPARE(model.data(speed, QmlRealtimeModel::MinimumValueRole).toDouble(), 1400.0);
    QCOMPARE(model.data(speed, QmlRealtimeModel::MaximumValueRole).toDouble(), 1500.0);
    QCOMPARE(model.data(speed, QmlRealtimeModel::AverageValueRole).toDouble(), 1442.5);
    QCOMPARE(model.data(speed, QmlRealtimeModel::QualityRole).toInt(),
             static_cast<int>(DataQuality::Good));
    QCOMPARE(model.data(speed, QmlRealtimeModel::QualityTextRole).toString(),
             QStringLiteral("正常 (Good)"));
    QVERIFY(!model.data(speed, QmlRealtimeModel::TimestampTextRole).toString().isEmpty());
}

void QmlRealtimeModelTest::ignoresUnknownTags()
{
    QmlRealtimeModel model;
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    RealtimeSnapshot snapshot;
    snapshot.tagId = QStringLiteral("not-configured");
    snapshot.timestampUtc = QDateTime::currentDateTimeUtc();

    model.applySnapshots({snapshot});

    QCOMPARE(changedSpy.size(), 0);
}

QTEST_GUILESS_MAIN(QmlRealtimeModelTest)

#include "tst_qml_realtime_model.moc"
