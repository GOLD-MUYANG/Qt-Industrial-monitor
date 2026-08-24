#include <QtTest>

#include "RollingStatistics.h"

class RollingStatisticsTest final : public QObject
{
    Q_OBJECT

private slots:
    void calculatesWindowValues();
    void expiresSamplesOlderThanSixtySeconds();
    void capsWindowAtOneHundredTwentySamples();
};

void RollingStatisticsTest::calculatesWindowValues()
{
    RollingStatistics window;
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    window.add(10.0, base);
    window.add(20.0, base.addMSecs(500));

    QCOMPARE(window.count(), 2);
    QCOMPARE(window.current(), 20.0);
    QCOMPARE(window.minimum(), 10.0);
    QCOMPARE(window.maximum(), 20.0);
    QCOMPARE(window.average(), 15.0);
}

void RollingStatisticsTest::expiresSamplesOlderThanSixtySeconds()
{
    RollingStatistics window;
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    window.add(10.0, base);
    window.add(20.0, base.addMSecs(500));
    window.add(30.0, base.addSecs(61));

    QCOMPARE(window.count(), 1);
    QCOMPARE(window.average(), 30.0);
}

void RollingStatisticsTest::capsWindowAtOneHundredTwentySamples()
{
    RollingStatistics window;
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    for (int index = 0; index < 121; ++index) {
        window.add(index, base.addMSecs(index));
    }

    QCOMPARE(window.count(), 120);
    QCOMPARE(window.minimum(), 1.0);
    QCOMPARE(window.maximum(), 120.0);
    QCOMPARE(window.average(), 60.5);
}

QTEST_GUILESS_MAIN(RollingStatisticsTest)

#include "tst_rolling_statistics.moc"
