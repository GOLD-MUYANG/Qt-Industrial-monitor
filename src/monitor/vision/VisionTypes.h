#pragma once

#include <QImage>
#include <QMetaType>
#include <QString>

namespace industrial::monitor::vision {

enum class VisionPlaybackState
{
    Idle,
    Ready,
    Playing,
    Paused,
    Finished,
    Error
};

struct VisionSourceInfo
{
    QString canonicalPath;
    QString displayName;
    double framesPerSecond = 30.0;
    qint64 totalFrames = -1;
    qint64 estimatedDurationMs = -1;
};

struct VisionFrameResult
{
    QImage image;
    qint64 positionMs = 0;
    qint64 frameNumber = 0;
    int targetCount = 0;
    double detectionTimeMs = 0.0;
};

void registerVisionMetaTypes();

} // namespace industrial::monitor::vision

Q_DECLARE_METATYPE(industrial::monitor::vision::VisionPlaybackState)
Q_DECLARE_METATYPE(industrial::monitor::vision::VisionSourceInfo)
Q_DECLARE_METATYPE(industrial::monitor::vision::VisionFrameResult)
