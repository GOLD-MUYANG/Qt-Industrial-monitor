#pragma once

#include "ColorObjectDetector.h"
#include "VisionTypes.h"

#include <QObject>

#include <memory>

class QTimer;

namespace cv {
class VideoCapture;
}

namespace industrial::monitor::vision {

class VideoFileWorker final : public QObject
{
    Q_OBJECT

public:
    explicit VideoFileWorker(QObject *parent = nullptr);
    ~VideoFileWorker() override;

public slots:
    void initialize();
    void openVideo(const QString &path);
    void play();
    void pause();
    void stop();
    void shutdown();

signals:
    void initialized(quintptr threadId);
    void sourceOpened(const industrial::monitor::vision::VisionSourceInfo &source);
    void frameReady(const industrial::monitor::vision::VisionFrameResult &frame);
    void stateChanged(industrial::monitor::vision::VisionPlaybackState state,
                      const QString &message);
    void videoError(const QString &message);
    void shutdownFinished();

private slots:
    void readNextFrame();

private:
    void startPlaybackTimer();
    void processAndEmitFrame(const cv::Mat &frame);
    void finishPlayback(const QString &message);
    void fail(const QString &message);
    void resetCurrentSource(bool clearReplayPath);
    void setState(VisionPlaybackState state, const QString &message);
    qint64 currentPositionMs();

    std::unique_ptr<cv::VideoCapture> m_capture;
    QTimer *m_timer = nullptr;
    ColorObjectDetector m_detector;
    VisionSourceInfo m_source;
    QString m_replayPath;
    VisionPlaybackState m_state = VisionPlaybackState::Idle;
    qint64 m_processedFrames = 0;
    qint64 m_lastPositionMs = 0;
    bool m_initialized = false;
};

} // namespace industrial::monitor::vision
