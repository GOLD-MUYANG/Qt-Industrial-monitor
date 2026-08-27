#include <QtTest>

#include "VisionPlaybackPolicy.h"

using namespace industrial::monitor::vision;

class VisionPlaybackPolicyTest final : public QObject
{
    Q_OBJECT

private slots:
    void fallsBackWhenBackendPositionIsZeroOrNonMonotonic();
    void acceptsSmallMetadataDriftAtNaturalEnd();
    void rejectsFailuresClearlyBeforeKnownEnd();
    void treatsUnknownFrameCountAsUncertainFinish();
};

void VisionPlaybackPolicyTest::fallsBackWhenBackendPositionIsZeroOrNonMonotonic()
{
    QCOMPARE(VisionPlaybackPolicy::positionMs(0.0, 1, 20.0, 0), 0LL);
    QCOMPARE(VisionPlaybackPolicy::positionMs(0.0, 2, 20.0, 0), 50LL);
    QCOMPARE(VisionPlaybackPolicy::positionMs(80.0, 3, 20.0, 50), 80LL);
    QCOMPARE(VisionPlaybackPolicy::positionMs(80.0, 4, 20.0, 80), 150LL);
    QCOMPARE(VisionPlaybackPolicy::positionMs(70.0, 4, 20.0, 80), 150LL);
}

void VisionPlaybackPolicyTest::acceptsSmallMetadataDriftAtNaturalEnd()
{
    QCOMPARE(VisionPlaybackPolicy::classifyReadFailure(
                 95, 100, 20.0, 95.0, 4'750.0, 5'000),
             VideoReadFailureDisposition::Finished);
    QCOMPARE(VisionPlaybackPolicy::classifyReadFailure(
                 90, 100, 20.0, 99.0, 4'950.0, 5'000),
             VideoReadFailureDisposition::Finished);
}

void VisionPlaybackPolicyTest::rejectsFailuresClearlyBeforeKnownEnd()
{
    QCOMPARE(VisionPlaybackPolicy::classifyReadFailure(
                 90, 100, 20.0, 90.0, 4'500.0, 5'000),
             VideoReadFailureDisposition::EarlyFailure);
}

void VisionPlaybackPolicyTest::treatsUnknownFrameCountAsUncertainFinish()
{
    QCOMPARE(VisionPlaybackPolicy::classifyReadFailure(
                 20, -1, 30.0, 0.0, 0.0, -1),
             VideoReadFailureDisposition::FinishedWithUncertainBackend);
}

QTEST_GUILESS_MAIN(VisionPlaybackPolicyTest)

#include "tst_vision_playback_policy.moc"
