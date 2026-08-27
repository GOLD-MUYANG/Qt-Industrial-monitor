#include "VisionFrameConverter.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <stdexcept>

namespace industrial::monitor::vision {

QImage VisionFrameConverter::toOwnedQImage(const cv::Mat &bgrFrame)
{
    if (bgrFrame.empty()) {
        throw std::invalid_argument("图像转换输入不能为空");
    }
    if (bgrFrame.type() != CV_8UC3) {
        throw std::invalid_argument("图像转换输入必须是 CV_8UC3 BGR 图像");
    }

    cv::Mat rgbFrame;
    cv::cvtColor(bgrFrame, rgbFrame, cv::COLOR_BGR2RGB);
    const QImage borrowed(rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
                          static_cast<int>(rgbFrame.step), QImage::Format_RGB888);
    return borrowed.copy();
}

} // namespace industrial::monitor::vision
