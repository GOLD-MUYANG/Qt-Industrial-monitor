#include <QtTest>

#include "ColorObjectDetector.h"

#include <opencv2/imgproc.hpp>

using namespace industrial::monitor::vision;

class ColorObjectDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsNoTargetsForBlackOrBlueFrames();
    void detectsOneLargeRedRectangleAndDrawsItsBox();
    void filtersSmallRedNoise();
    void countsSeparatedTargetsAndBothHueEnds();
    void scalesWideFramesWithoutModifyingInput();
};

void ColorObjectDetectorTest::returnsNoTargetsForBlackOrBlueFrames()
{
    const ColorObjectDetector detector;
    const cv::Mat black = cv::Mat::zeros(240, 320, CV_8UC3);
    cv::Mat blue = black.clone();
    cv::rectangle(blue, cv::Rect(40, 50, 80, 70), cv::Scalar(255, 0, 0), cv::FILLED);

    QCOMPARE(detector.process(black).targetCount(), 0);
    QCOMPARE(detector.process(blue).targetCount(), 0);
}

void ColorObjectDetectorTest::detectsOneLargeRedRectangleAndDrawsItsBox()
{
    const ColorObjectDetector detector;
    cv::Mat frame = cv::Mat::zeros(240, 320, CV_8UC3);
    const cv::Rect expectedBox(30, 40, 80, 70);
    cv::rectangle(frame, expectedBox, cv::Scalar(0, 0, 255), cv::FILLED);

    const ColorDetectionResult result = detector.process(frame);

    QCOMPARE(result.targetCount(), 1);
    QCOMPARE(result.boxes.front(), expectedBox);
    const cv::Vec3b greenEdge = result.annotatedFrame.at<cv::Vec3b>(40, 30);
    QCOMPARE(greenEdge, cv::Vec3b(0, 255, 0));
}

void ColorObjectDetectorTest::filtersSmallRedNoise()
{
    const ColorObjectDetector detector;
    cv::Mat frame = cv::Mat::zeros(120, 160, CV_8UC3);
    cv::rectangle(frame, cv::Rect(10, 10, 15, 15), cv::Scalar(0, 0, 255), cv::FILLED);

    QCOMPARE(detector.process(frame).targetCount(), 0);
}

void ColorObjectDetectorTest::countsSeparatedTargetsAndBothHueEnds()
{
    const ColorObjectDetector detector;
    cv::Mat hsv = cv::Mat::zeros(180, 320, CV_8UC3);
    cv::rectangle(hsv, cv::Rect(20, 30, 60, 60), cv::Scalar(0, 255, 255), cv::FILLED);
    cv::rectangle(hsv, cv::Rect(220, 80, 60, 60), cv::Scalar(179, 255, 255), cv::FILLED);
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);

    const ColorDetectionResult result = detector.process(bgr);

    QCOMPARE(result.targetCount(), 2);
    QCOMPARE(result.boxes.at(0), cv::Rect(20, 30, 60, 60));
    QCOMPARE(result.boxes.at(1), cv::Rect(220, 80, 60, 60));
}

void ColorObjectDetectorTest::scalesWideFramesWithoutModifyingInput()
{
    const ColorObjectDetector detector;
    cv::Mat frame = cv::Mat::zeros(800, 1600, CV_8UC3);
    cv::rectangle(frame, cv::Rect(400, 200, 400, 300), cv::Scalar(0, 0, 255), cv::FILLED);
    const cv::Mat original = frame.clone();

    const ColorDetectionResult result = detector.process(frame);

    QCOMPARE(result.annotatedFrame.cols, 1280);
    QCOMPARE(result.annotatedFrame.rows, 640);
    QCOMPARE(result.targetCount(), 1);
    QCOMPARE(cv::norm(frame, original, cv::NORM_INF), 0.0);
}

QTEST_GUILESS_MAIN(ColorObjectDetectorTest)

#include "tst_color_object_detector.moc"
