#include "VisionPlaybackPolicy.h"

#include <algorithm>
#include <cmath>

namespace industrial::monitor::vision {
namespace {

constexpr double kFallbackFramesPerSecond = 30.0;
constexpr qint64 kEndToleranceMs = 250;

double usableFramesPerSecond(double framesPerSecond)
{
    return std::isfinite(framesPerSecond) && framesPerSecond >= 1.0
        ? framesPerSecond
        : kFallbackFramesPerSecond;
}

} // namespace

qint64 VisionPlaybackPolicy::positionMs(double reportedPositionMs,
                                        qint64 processedFrames,
                                        double framesPerSecond,
                                        qint64 lastPositionMs)
{
    const double fps = usableFramesPerSecond(framesPerSecond);
    const qint64 estimatedPosition = static_cast<qint64>(std::llround(
        std::max<qint64>(0, processedFrames - 1) * 1000.0 / fps));
    if (!std::isfinite(reportedPositionMs) || reportedPositionMs < 0.0) {
        return std::max(lastPositionMs, estimatedPosition);
    }

    const qint64 reported = static_cast<qint64>(std::llround(reportedPositionMs));
    if ((processedFrames > 1 && reported == 0)
        || reported < lastPositionMs
        || (reported == lastPositionMs && estimatedPosition > lastPositionMs)) {
        return std::max(lastPositionMs, estimatedPosition);
    }
    return std::max(lastPositionMs, reported);
}

VideoReadFailureDisposition VisionPlaybackPolicy::classifyReadFailure(
    qint64 processedFrames,
    qint64 totalFrames,
    double framesPerSecond,
    double reportedFramePosition,
    double reportedPositionMs,
    qint64 estimatedDurationMs)
{
    if (totalFrames <= 0) {
        return VideoReadFailureDisposition::FinishedWithUncertainBackend;
    }

    const double fps = usableFramesPerSecond(framesPerSecond);
    const qint64 frameTolerance = std::max<qint64>(
        2, static_cast<qint64>(std::ceil(fps * kEndToleranceMs / 1000.0)));
    double observedFrames = static_cast<double>(processedFrames);
    if (std::isfinite(reportedFramePosition) && reportedFramePosition >= 0.0) {
        observedFrames = std::max(observedFrames, reportedFramePosition);
    }
    if (observedFrames + frameTolerance >= totalFrames) {
        return VideoReadFailureDisposition::Finished;
    }

    if (estimatedDurationMs > 0
        && std::isfinite(reportedPositionMs)
        && reportedPositionMs > 0.0
        && reportedPositionMs + kEndToleranceMs >= estimatedDurationMs) {
        return VideoReadFailureDisposition::Finished;
    }
    return VideoReadFailureDisposition::EarlyFailure;
}

} // namespace industrial::monitor::vision
