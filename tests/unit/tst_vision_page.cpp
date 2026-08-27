#include <QtTest>

#include "VisionPage.h"

#include <QLabel>
#include <QPushButton>

using namespace industrial::monitor::vision;

class VisionPageTest final : public QObject
{
    Q_OBJECT

private slots:
    void appliesButtonMatrixFromPlaybackState();
    void emitsPlaybackCommandsWithoutOpeningFileDialog();
    void rendersFrameFactsAndInlineErrors();
};

void VisionPageTest::appliesButtonMatrixFromPlaybackState()
{
    VisionPage page;
    auto *select = page.findChild<QPushButton *>(QStringLiteral("visionSelectButton"));
    auto *play = page.findChild<QPushButton *>(QStringLiteral("visionPlayButton"));
    auto *pause = page.findChild<QPushButton *>(QStringLiteral("visionPauseButton"));
    auto *stop = page.findChild<QPushButton *>(QStringLiteral("visionStopButton"));
    QVERIFY(select);
    QVERIFY(play);
    QVERIFY(pause);
    QVERIFY(stop);

    QVERIFY(select->isEnabled());
    QVERIFY(!play->isEnabled());
    QVERIFY(!pause->isEnabled());
    QVERIFY(!stop->isEnabled());

    page.setPlaybackState(VisionPlaybackState::Ready, QStringLiteral("已就绪"));
    QVERIFY(play->isEnabled());
    QCOMPARE(play->text(), QStringLiteral("播放"));
    QVERIFY(!pause->isEnabled());
    QVERIFY(stop->isEnabled());

    page.setPlaybackState(VisionPlaybackState::Playing, QStringLiteral("播放中"));
    QVERIFY(!play->isEnabled());
    QVERIFY(pause->isEnabled());
    QVERIFY(stop->isEnabled());

    page.setPlaybackState(VisionPlaybackState::Paused, QStringLiteral("已暂停"));
    QVERIFY(play->isEnabled());
    QCOMPARE(play->text(), QStringLiteral("继续"));
    QVERIFY(!pause->isEnabled());

    page.setPlaybackState(VisionPlaybackState::Finished, QStringLiteral("已结束"));
    QVERIFY(play->isEnabled());
    QCOMPARE(play->text(), QStringLiteral("重播"));
    QVERIFY(!pause->isEnabled());

    page.setPlaybackState(VisionPlaybackState::Error, QStringLiteral("打开失败"));
    QVERIFY(!play->isEnabled());
    QVERIFY(!pause->isEnabled());
    QVERIFY(stop->isEnabled());
}

void VisionPageTest::emitsPlaybackCommandsWithoutOpeningFileDialog()
{
    VisionPage page;
    auto *play = page.findChild<QPushButton *>(QStringLiteral("visionPlayButton"));
    auto *pause = page.findChild<QPushButton *>(QStringLiteral("visionPauseButton"));
    auto *stop = page.findChild<QPushButton *>(QStringLiteral("visionStopButton"));
    QSignalSpy playSpy(&page, &VisionPage::playRequested);
    QSignalSpy pauseSpy(&page, &VisionPage::pauseRequested);
    QSignalSpy stopSpy(&page, &VisionPage::stopRequested);

    page.setPlaybackState(VisionPlaybackState::Ready, {});
    QTest::mouseClick(play, Qt::LeftButton);
    QCOMPARE(playSpy.count(), 1);
    page.setPlaybackState(VisionPlaybackState::Playing, {});
    QTest::mouseClick(pause, Qt::LeftButton);
    QCOMPARE(pauseSpy.count(), 1);
    QTest::mouseClick(stop, Qt::LeftButton);
    QCOMPARE(stopSpy.count(), 1);
}

void VisionPageTest::rendersFrameFactsAndInlineErrors()
{
    VisionPage page;
    auto *target = page.findChild<QLabel *>(QStringLiteral("visionTargetLabel"));
    auto *detectionTime = page.findChild<QLabel *>(QStringLiteral("visionDetectionTimeLabel"));
    auto *error = page.findChild<QLabel *>(QStringLiteral("visionErrorLabel"));
    auto *frame = page.findChild<QLabel *>(QStringLiteral("visionFrameLabel"));
    QVERIFY(target);
    QVERIFY(detectionTime);
    QVERIFY(error);
    QVERIFY(frame);

    VisionFrameResult result;
    result.image = QImage(32, 24, QImage::Format_RGB888);
    result.image.fill(Qt::black);
    result.targetCount = 0;
    result.detectionTimeMs = 2.75;
    page.setFrame(result);
    QVERIFY(target->text().contains(QStringLiteral("未检测到红色目标")));
    QVERIFY(detectionTime->text().contains(QStringLiteral("2.75")));
    QVERIFY(!frame->pixmap().isNull());

    result.targetCount = 2;
    page.setFrame(result);
    QVERIFY(target->text().contains(QStringLiteral("2 个")));

    page.showError(QStringLiteral("测试视频无法解码"));
    QVERIFY(error->text().contains(QStringLiteral("测试视频无法解码")));
    QVERIFY(!error->isHidden());
}

QTEST_MAIN(VisionPageTest)

#include "tst_vision_page.moc"
