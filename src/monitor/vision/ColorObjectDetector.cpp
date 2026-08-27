#include "ColorObjectDetector.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace industrial::monitor::vision
{
namespace
{

constexpr int kMaximumFrameWidth = 1280;
constexpr double kMinimumContourArea = 500.0;

// 缩小大图
cv::Mat makeWorkingFrame(const cv::Mat &input)
{
    if (input.empty())
    {
        throw std::invalid_argument("检测输入不能为空");
    }
    if (input.type() != CV_8UC3)
    {
        throw std::invalid_argument("检测输入必须是 CV_8UC3 BGR 图像");
    }
    if (input.cols <= kMaximumFrameWidth)
    {
        return input.clone();
    }

    const double scale = static_cast<double>(kMaximumFrameWidth) / input.cols;
    const int scaledHeight = std::max(1, static_cast<int>(std::lround(input.rows * scale)));
    cv::Mat resized;
    cv::resize(input, resized, cv::Size(kMaximumFrameWidth, scaledHeight), 0.0, 0.0,
               cv::INTER_AREA);
    return resized;
}

} // namespace

// 这一块儿属于是算法的相关知识，暂时不看了
// 大体上就是转换大小，转颜色格式，提取红色像素，去噪点，查找连续区域，过滤小的区域，计算外面的矩形，画矩形并计数。
ColorDetectionResult ColorObjectDetector::process(const cv::Mat &input) const
{
    // 第一块：生成固定阈值的红色掩膜，并用开闭操作抑制小噪点和孔洞。
    ColorDetectionResult result;
    result.annotatedFrame = makeWorkingFrame(input);

    cv::Mat hsv;
    // BGR 转成 HSV，HSV更适合按颜色筛选。
    /**
    H：颜色种类
    S：颜色饱和度
    V：亮度
    */
    cv::cvtColor(result.annotatedFrame, hsv, cv::COLOR_BGR2HSV);
    cv::Mat lowRedMask;
    cv::Mat highRedMask;
    // 红色在hsv的表示中有两段都是红色，所以划了两个范围分别提取
    cv::inRange(hsv, cv::Scalar(0, 100, 80), cv::Scalar(10, 255, 255), lowRedMask);
    cv::inRange(hsv, cv::Scalar(170, 100, 80), cv::Scalar(179, 255, 255), highRedMask);

    cv::Mat redMask;
    // 合并两个范围的掩膜
    cv::bitwise_or(lowRedMask, highRedMask, redMask);
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    // 去小噪声（去掉很零散的红色像素）
    cv::morphologyEx(redMask, redMask, cv::MORPH_OPEN, kernel);
    // 补小孔洞（一堆红色中间出了几个不是红色的小像素，也认为他们是连续的）
    cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, kernel);

    // 第二块：提取、过滤并稳定排序目标，保证画框编号可重复。
    std::vector<std::vector<cv::Point>> contours;

    // 自动圈出来几个红色区域的矩形，放到vector里
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto &contour : contours)
    {
        // 找到的矩形面积要大于500
        if (cv::contourArea(contour) >= kMinimumContourArea)
        {
            result.boxes.push_back(cv::boundingRect(contour));
        }
    }
    std::sort(result.boxes.begin(), result.boxes.end(),
              [](const cv::Rect &left, const cv::Rect &right)
              { return left.x == right.x ? left.y < right.y : left.x < right.x; });

    // 第三块：检测框与 ASCII 标签只写入独立工作帧，不修改调用方输入。
    for (std::size_t index = 0; index < result.boxes.size(); ++index)
    {
        const cv::Rect &box = result.boxes.at(index);
        cv::rectangle(result.annotatedFrame, box, cv::Scalar(0, 255, 0), 2);
        const std::string label = "Red #" + std::to_string(index + 1);
        const int labelY = std::max(18, box.y - 6);
        cv::putText(result.annotatedFrame, label, cv::Point(box.x, labelY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    }
    return result;
}

} // namespace industrial::monitor::vision
