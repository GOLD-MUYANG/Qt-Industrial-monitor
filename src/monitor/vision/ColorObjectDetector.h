#pragma once

#include <opencv2/core.hpp>

#include <vector>

namespace industrial::monitor::vision {

struct ColorDetectionResult
{
    cv::Mat annotatedFrame;
    std::vector<cv::Rect> boxes;

    int targetCount() const
    {
        return static_cast<int>(boxes.size());
    }
};

class ColorObjectDetector final
{
public:
    ColorDetectionResult process(const cv::Mat &input) const;
};

} // namespace industrial::monitor::vision
