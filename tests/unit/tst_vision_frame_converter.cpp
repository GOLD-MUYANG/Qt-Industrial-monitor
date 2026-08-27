#include <QtTest>

#include "VisionFrameConverter.h"

#include <QColor>

#include <opencv2/core.hpp>

#include <stdexcept>

using namespace industrial::monitor::vision;

class VisionFrameConverterTest final : public QObject
{
    Q_OBJECT

private slots:
    void preservesRedWhenConvertingBgrToRgb();
    void rejectsEmptyAndNonBgrInput();
    void returnsPixelsIndependentFromSourceMat();
};

void VisionFrameConverterTest::preservesRedWhenConvertingBgrToRgb()
{
    const cv::Mat redBgr(1, 1, CV_8UC3, cv::Scalar(0, 0, 255));

    const QImage image = VisionFrameConverter::toOwnedQImage(redBgr);

    QCOMPARE(image.format(), QImage::Format_RGB888);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
}

void VisionFrameConverterTest::rejectsEmptyAndNonBgrInput()
{
    const cv::Mat empty;
    const cv::Mat grayscale(4, 4, CV_8UC1, cv::Scalar(127));

    QVERIFY_EXCEPTION_THROWN(VisionFrameConverter::toOwnedQImage(empty),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(VisionFrameConverter::toOwnedQImage(grayscale),
                             std::invalid_argument);
}

void VisionFrameConverterTest::returnsPixelsIndependentFromSourceMat()
{
    cv::Mat source(2, 2, CV_8UC3, cv::Scalar(0, 0, 255));
    const QImage image = VisionFrameConverter::toOwnedQImage(source);

    source.setTo(cv::Scalar(255, 0, 0));
    source.release();

    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(1, 1), QColor(Qt::red));
}

QTEST_GUILESS_MAIN(VisionFrameConverterTest)

#include "tst_vision_frame_converter.moc"
