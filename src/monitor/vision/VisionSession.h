#pragma once

#include "VisionTypes.h"

#include <QObject>
#include <QPointer>

class QThread;

namespace industrial::monitor::vision {

class VideoFileWorker;

class VisionSession final : public QObject
{
    Q_OBJECT

public:
    explicit VisionSession(QObject *parent = nullptr);
    ~VisionSession() override;

    bool start();
    bool shutdown(int timeoutMs = 3'000);
    bool isRunning() const;

public slots:
    void openVideo(const QString &path);
    void play();
    void pause();
    void stop();

signals:
    void sourceOpened(const industrial::monitor::vision::VisionSourceInfo &source);
    void frameReady(const industrial::monitor::vision::VisionFrameResult &frame);
    void stateChanged(industrial::monitor::vision::VisionPlaybackState state,
                      const QString &message);
    void videoError(const QString &message);
    void workerInitialized(quintptr threadId);
    void stopped();

    // 内部命令只投递到视觉线程，不向 UI 暴露 Worker 指针。
    void openVideoRequested(const QString &path);
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void shutdownRequested();

private:
    QPointer<QThread> m_thread;
    QPointer<VideoFileWorker> m_worker;
};

} // namespace industrial::monitor::vision
