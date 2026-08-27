#pragma once

#include <QtGlobal>

namespace industrial::monitor::vision {

enum class VideoReadFailureDisposition
{
    Finished,
    FinishedWithUncertainBackend,
    EarlyFailure
};

class VisionPlaybackPolicy final
{
public:
    static qint64 positionMs(double reportedPositionMs,
                             qint64 processedFrames,
                             double framesPerSecond,
                             qint64 lastPositionMs);

    static VideoReadFailureDisposition classifyReadFailure(
        qint64 processedFrames,
        qint64 totalFrames,
        double framesPerSecond,
        double reportedFramePosition,
        double reportedPositionMs,
        qint64 estimatedDurationMs);
};

} // namespace industrial::monitor::vision
