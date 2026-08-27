#include "QmlTrendModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace industrial::protocol;

namespace {

RealtimeSnapshot snapshot(const QString &tagId, qint64 offsetMs, double value)
{
    RealtimeSnapshot result;
    result.deviceId = QStringLiteral("device-1");
    result.tagId = tagId;
    result.current = value;
    result.quality = DataQuality::Good;
    result.timestampUtc =
        QDateTime::fromMSecsSinceEpoch(1'700'000'000'000LL + offsetMs, Qt::UTC);
    return result;
}

} // namespace

class QmlTrendModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesTwoNumericColumnsAndSwitchesTags();
    void keepsOnlySixtySecondsAndOneHundredTwentyPoints();
    void cachesWithoutNotifyingWhilePaused();
    void ignoresStatusAndBadTimestamps();
};

void QmlTrendModelTest::exposesTwoNumericColumnsAndSwitchesTags()
{
    QmlTrendModel model;
    QCOMPARE(model.columnCount(), 2);
    QCOMPARE(model.selectedTagId(), QStringLiteral("temperature"));

    model.applySnapshots({snapshot(QStringLiteral("temperature"), 0, 20.5),
                          snapshot(QStringLiteral("pressure"), 0, 1.25)});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 1)).toDouble(), 20.5);

    model.selectTag(QStringLiteral("pressure"));
    QCOMPARE(model.selectedTagId(), QStringLiteral("pressure"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 1)).toDouble(), 1.25);
    QCOMPARE(model.data(model.index(0, 0)).metaType().id(), QMetaType::Double);
}

void QmlTrendModelTest::keepsOnlySixtySecondsAndOneHundredTwentyPoints()
{
    QmlTrendModel model;
    for (int index = 0; index < 130; ++index)
    {
        model.applySnapshots(
            {snapshot(QStringLiteral("temperature"), index * 500, index)});
    }

    QCOMPARE(model.rowCount(), 120);
    QCOMPARE(model.data(model.index(0, 1)).toDouble(), 10.0);
    QCOMPARE(model.data(model.index(119, 1)).toDouble(), 129.0);

    model.applySnapshots(
        {snapshot(QStringLiteral("temperature"), 130'000, 260.0)});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 1)).toDouble(), 260.0);
}

void QmlTrendModelTest::cachesWithoutNotifyingWhilePaused()
{
    QmlTrendModel model;
    model.applySnapshots({snapshot(QStringLiteral("temperature"), 0, 20.0)});
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &QmlTrendModel::pointCountChanged);

    model.setDisplayPaused(true);
    model.applySnapshots({snapshot(QStringLiteral("temperature"), 500, 21.0)});

    // 后台缓存继续采集，但映射给图表的模型必须保持冻结。
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(resetSpy.size(), 0);
    QCOMPARE(countSpy.size(), 0);

    model.setDisplayPaused(false);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(resetSpy.size(), 1);
    QCOMPARE(countSpy.size(), 1);
}

void QmlTrendModelTest::ignoresStatusAndBadTimestamps()
{
    QmlTrendModel model;
    auto badTimestamp = snapshot(QStringLiteral("temperature"), 0, 1.0);
    badTimestamp.timestampUtc = {};

    model.applySnapshots({snapshot(QStringLiteral("status"), 0, 1.0), badTimestamp});

    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(QmlTrendModelTest)

#include "tst_qml_trend_model.moc"
