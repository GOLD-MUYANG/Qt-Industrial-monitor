#include "VisionSession.h"

#include "VideoFileWorker.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

namespace industrial::monitor::vision {

VisionSession::VisionSession(QObject *parent)
    : QObject(parent)
{
}

VisionSession::~VisionSession()
{
    if (!isRunning()) {
        return;
    }
    if (!shutdown(3'000)) {
        // Worker 保留 shutdownFinished -> quit -> deleteLater 自主退出链。
        // QObject 析构会断开回到本 Session 的连接，避免悬空回调。
        if (m_thread) {
            QObject::disconnect(m_thread, nullptr, this, nullptr);
        }
        if (m_worker) {
            QObject::disconnect(m_worker, nullptr, this, nullptr);
        }
    }
}

bool VisionSession::start()
{
    if (isRunning() || m_thread || m_worker) {
        return false;
    }
    registerVisionMetaTypes();

    auto *thread = new QThread;
    auto *worker = new VideoFileWorker;
    thread->setObjectName(QStringLiteral("opencv-video-processing"));
    m_thread = thread;
    m_worker = worker;

    // 第一块：先建立生命周期与命令通道，再移动无 parent Worker。
    connect(thread, &QThread::started,
            worker, &VideoFileWorker::initialize,
            Qt::QueuedConnection);
    connect(this, &VisionSession::openVideoRequested,
            worker, &VideoFileWorker::openVideo,
            Qt::QueuedConnection);
    connect(this, &VisionSession::playRequested,
            worker, &VideoFileWorker::play,
            Qt::QueuedConnection);
    connect(this, &VisionSession::pauseRequested,
            worker, &VideoFileWorker::pause,
            Qt::QueuedConnection);
    connect(this, &VisionSession::stopRequested,
            worker, &VideoFileWorker::stop,
            Qt::QueuedConnection);
    connect(this, &VisionSession::shutdownRequested,
            worker, &VideoFileWorker::shutdown,
            Qt::QueuedConnection);

    // 第二块：结果只以已注册 Qt 值对象返回主线程。
    connect(worker, &VideoFileWorker::initialized,
            this, &VisionSession::workerInitialized,
            Qt::QueuedConnection);
    connect(worker, &VideoFileWorker::sourceOpened,
            this, &VisionSession::sourceOpened,
            Qt::QueuedConnection);
    connect(worker, &VideoFileWorker::frameReady,
            this, &VisionSession::frameReady,
            Qt::QueuedConnection);
    connect(worker, &VideoFileWorker::stateChanged,
            this, &VisionSession::stateChanged,
            Qt::QueuedConnection);
    connect(worker, &VideoFileWorker::videoError,
            this, &VisionSession::videoError,
            Qt::QueuedConnection);

    // 第三块：Worker 先释放 Capture/Timer，再让线程退出并在原线程销毁。
    connect(worker, &VideoFileWorker::shutdownFinished,
            thread, &QThread::quit,
            Qt::DirectConnection);
    connect(thread, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,
            this,
            [this, thread]() {
                if (m_thread == thread) {
                    m_worker.clear();
                    m_thread.clear();
                }
                emit stopped();
            },
            Qt::QueuedConnection);
    connect(thread, &QThread::finished,
            thread, &QObject::deleteLater);

    worker->moveToThread(thread);
    thread->start();
    return true;
}

bool VisionSession::shutdown(int timeoutMs)
{
    if (!m_thread || !m_thread->isRunning()) {
        return true;
    }
    if (QThread::currentThread() == m_thread) {
        emit videoError(QStringLiteral("不能在视觉线程内等待自身退出"));
        return false;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(m_thread, &QThread::finished,
            &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout,
            &loop, &QEventLoop::quit);

    emit shutdownRequested();
    timeoutTimer.start(qMax(0, timeoutMs));
    if (m_thread && m_thread->isRunning()) {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    if (!m_thread || !m_thread->isRunning()) {
        return true;
    }

    emit videoError(QStringLiteral("视觉线程未在 %1 ms 内停止").arg(timeoutMs));
    return false;
}

bool VisionSession::isRunning() const
{
    return m_thread && m_thread->isRunning();
}

void VisionSession::openVideo(const QString &path)
{
    if (isRunning() && m_worker && !path.trimmed().isEmpty()) {
        emit openVideoRequested(path);
    }
}

void VisionSession::play()
{
    if (isRunning() && m_worker) {
        emit playRequested();
    }
}

void VisionSession::pause()
{
    if (isRunning() && m_worker) {
        emit pauseRequested();
    }
}

void VisionSession::stop()
{
    if (isRunning() && m_worker) {
        emit stopRequested();
    }
}

} // namespace industrial::monitor::vision
