#include <QtTest>

#include "VisionTypes.h"

using namespace industrial::monitor::vision;

class VisionTypesTest final : public QObject
{
    Q_OBJECT

private slots:
    void registersQueuedConnectionValueTypes();
    void usesExplicitDefaultsForUnknownProgress();
};

void VisionTypesTest::registersQueuedConnectionValueTypes()
{
    registerVisionMetaTypes();

    QVERIFY(QMetaType::fromName("industrial::monitor::vision::VisionPlaybackState").isValid());
    QVERIFY(QMetaType::fromName("industrial::monitor::vision::VisionSourceInfo").isValid());
    QVERIFY(QMetaType::fromName("industrial::monitor::vision::VisionFrameResult").isValid());
}

void VisionTypesTest::usesExplicitDefaultsForUnknownProgress()
{
    const VisionSourceInfo source;
    const VisionFrameResult frame;

    QCOMPARE(source.framesPerSecond, 30.0);
    QCOMPARE(source.totalFrames, -1LL);
    QCOMPARE(source.estimatedDurationMs, -1LL);
    QCOMPARE(frame.positionMs, 0LL);
    QCOMPARE(frame.frameNumber, 0LL);
    QCOMPARE(frame.targetCount, 0);
}

QTEST_GUILESS_MAIN(VisionTypesTest)

#include "tst_vision_types.moc"
