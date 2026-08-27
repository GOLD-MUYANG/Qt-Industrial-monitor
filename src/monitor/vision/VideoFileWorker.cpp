#include "VideoFileWorker.h"

#include "VisionFrameConverter.h"
#include "VisionPlaybackPolicy.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTimer>

#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cmath>
#include <exception>

namespace industrial::monitor::vision
{
namespace
{

constexpr double kFallbackFramesPerSecond = 30.0;
constexpr double kMaximumPlaybackFramesPerSecond = 60.0;

bool isPositiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

VideoFileWorker::VideoFileWorker(QObject *parent) : QObject(parent)
{
}

VideoFileWorker::~VideoFileWorker() = default;

void VideoFileWorker::initialize()
{
    if (m_initialized)
    {
        return;
    }

    // Capture 和 Timer 都在 moveToThread 后由本槽创建，线程归属不会泄漏到 UI。
    m_capture = std::make_unique<cv::VideoCapture>();
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &VideoFileWorker::readNextFrame);
    m_initialized = true;
    emit initialized(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

void VideoFileWorker::openVideo(const QString &path)
{
    if (!m_initialized || !m_capture || !m_timer)
    {
        fail(QStringLiteral("视觉 Worker 尚未初始化"));
        return;
    }

    resetCurrentSource(false);
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable())
    {
        fail(QStringLiteral("视频路径不可访问：%1").arg(path));
        return;
    }

    try
    {
        const QByteArray encodedPath = QFile::encodeName(fileInfo.absoluteFilePath());
        if (!m_capture->open(encodedPath.constData()) || !m_capture->isOpened())
        {
            fail(QStringLiteral(
                "无法打开视频或当前后端不支持该格式；请尝试 H.264 MP4 或 MJPEG AVI"));
            return;
        }

        const double reportedFps = m_capture->get(cv::CAP_PROP_FPS);
        const bool fpsIsValid = std::isfinite(reportedFps) && reportedFps >= 1.0;
        m_source.framesPerSecond = fpsIsValid
                                       ? std::min(reportedFps, kMaximumPlaybackFramesPerSecond)
                                       : kFallbackFramesPerSecond;

        const double reportedFrameCount = m_capture->get(cv::CAP_PROP_FRAME_COUNT);
        if (isPositiveFinite(reportedFrameCount))
        {
            m_source.totalFrames = static_cast<qint64>(std::llround(reportedFrameCount));
        }
        if (fpsIsValid && m_source.totalFrames > 0)
        {
            m_source.estimatedDurationMs =
                static_cast<qint64>(std::llround(m_source.totalFrames * 1000.0 / reportedFps));
        }

        const QString canonicalPath = fileInfo.canonicalFilePath().isEmpty()
                                          ? fileInfo.absoluteFilePath()
                                          : fileInfo.canonicalFilePath();
        m_source.canonicalPath = canonicalPath;
        m_source.displayName = fileInfo.fileName();

        cv::Mat firstFrame;
        if (!m_capture->read(firstFrame) || firstFrame.empty())
        {
            fail(QStringLiteral("视频已打开，但无法读取首帧；请检查容器和编解码器支持"));
            return;
        }

        m_replayPath = canonicalPath;
        m_processedFrames = 1;
        emit sourceOpened(m_source);
        processAndEmitFrame(firstFrame);
        setState(VisionPlaybackState::Ready, QStringLiteral("视频已就绪"));
    }
    catch (const cv::Exception &error)
    {
        fail(QStringLiteral("OpenCV 打开或处理视频失败：%1")
                 .arg(QString::fromLocal8Bit(error.what())));
    }
    catch (const std::exception &error)
    {
        fail(QStringLiteral("视频处理失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
}

void VideoFileWorker::play()
{
    if (m_state == VisionPlaybackState::Finished && !m_replayPath.isEmpty())
    {
        const QString replayPath = m_replayPath;
        openVideo(replayPath);
        if (m_state == VisionPlaybackState::Ready)
        {
            startPlaybackTimer();
        }
        return;
    }
    if (m_state == VisionPlaybackState::Ready || m_state == VisionPlaybackState::Paused)
    {
        startPlaybackTimer();
    }
}

void VideoFileWorker::pause()
{
    if (m_state != VisionPlaybackState::Playing || !m_timer)
    {
        return;
    }
    m_timer->stop();
    setState(VisionPlaybackState::Paused, QStringLiteral("视频已暂停"));
}

void VideoFileWorker::stop()
{
    resetCurrentSource(true);
    setState(VisionPlaybackState::Idle, QStringLiteral("视频已停止"));
}

void VideoFileWorker::shutdown()
{
    resetCurrentSource(true);
    emit shutdownFinished();
}

void VideoFileWorker::readNextFrame()
{
    if (m_state != VisionPlaybackState::Playing || !m_capture || !m_capture->isOpened())
    {
        return;
    }

    try
    {
        cv::Mat frame;
        if (!m_capture->read(frame) || frame.empty())
        {
            const auto disposition = VisionPlaybackPolicy::classifyReadFailure(
                m_processedFrames, m_source.totalFrames, m_source.framesPerSecond,
                m_capture->get(cv::CAP_PROP_POS_FRAMES), m_capture->get(cv::CAP_PROP_POS_MSEC),
                m_source.estimatedDurationMs);
            if (disposition == VideoReadFailureDisposition::FinishedWithUncertainBackend)
            {
                finishPlayback(QStringLiteral("视频结束或后端提前停止"));
            }
            else if (disposition == VideoReadFailureDisposition::Finished)
            {
                finishPlayback(QStringLiteral("视频播放结束"));
            }
            else
            {
                fail(QStringLiteral("视频在预计结束位置之前读取失败"));
            }
            return;
        }

        ++m_processedFrames;
        processAndEmitFrame(frame);
    }
    catch (const cv::Exception &error)
    {
        fail(QStringLiteral("OpenCV 逐帧处理失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    catch (const std::exception &error)
    {
        fail(QStringLiteral("逐帧处理失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
}

void VideoFileWorker::startPlaybackTimer()
{
    const int intervalMs =
        std::max(1, static_cast<int>(std::lround(1000.0 / m_source.framesPerSecond)));
    m_timer->start(intervalMs);
    setState(VisionPlaybackState::Playing, QStringLiteral("视频播放中"));
}

void VideoFileWorker::processAndEmitFrame(const cv::Mat &frame)
{
    QElapsedTimer detectionTimer;
    detectionTimer.start();
    const ColorDetectionResult detection = m_detector.process(frame);
    const double detectionTimeMs = detectionTimer.nsecsElapsed() / 1'000'000.0;

    VisionFrameResult result;
    // 把一帧的经过处理的画面转化成
    result.image = VisionFrameConverter::toOwnedQImage(detection.annotatedFrame);
    result.positionMs = currentPositionMs();
    result.frameNumber = m_processedFrames;
    result.targetCount = detection.targetCount();
    result.detectionTimeMs = detectionTimeMs;
    emit frameReady(result);
}

void VideoFileWorker::finishPlayback(const QString &message)
{
    if (m_timer)
    {
        m_timer->stop();
    }
    if (m_capture)
    {
        m_capture->release();
    }
    setState(VisionPlaybackState::Finished, message);
}

void VideoFileWorker::fail(const QString &message)
{
    if (m_timer)
    {
        m_timer->stop();
    }
    if (m_capture)
    {
        m_capture->release();
    }
    m_source = VisionSourceInfo{};
    m_processedFrames = 0;
    m_lastPositionMs = 0;
    setState(VisionPlaybackState::Error, message);
    emit videoError(message);
}

void VideoFileWorker::resetCurrentSource(bool clearReplayPath)
{
    if (m_timer)
    {
        m_timer->stop();
    }
    if (m_capture)
    {
        m_capture->release();
    }
    m_source = VisionSourceInfo{};
    m_processedFrames = 0;
    m_lastPositionMs = 0;
    if (clearReplayPath)
    {
        m_replayPath.clear();
    }
}

void VideoFileWorker::setState(VisionPlaybackState state, const QString &message)
{
    m_state = state;
    emit stateChanged(state, message);
}

qint64 VideoFileWorker::currentPositionMs()
{
    double reportedPosition = -1.0;
    if (m_capture && m_capture->isOpened())
    {
        reportedPosition = m_capture->get(cv::CAP_PROP_POS_MSEC);
    }
    m_lastPositionMs = VisionPlaybackPolicy::positionMs(reportedPosition, m_processedFrames,
                                                        m_source.framesPerSecond, m_lastPositionMs);
    return m_lastPositionMs;
}

} // namespace industrial::monitor::vision
