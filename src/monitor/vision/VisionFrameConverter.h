#pragma once

#include <QImage>

namespace cv {
class Mat;
}

namespace industrial::monitor::vision {

class VisionFrameConverter final
{
public:
    static QImage toOwnedQImage(const cv::Mat &bgrFrame);
};

} // namespace industrial::monitor::vision
