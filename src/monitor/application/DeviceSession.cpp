#include "DeviceSession.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

using namespace industrial::protocol;

DeviceSession::DeviceSession(IProtocolPlugin *plugin,
                             const DeviceConfig &config,
                             QObject *parent)
    : QObject(parent)
    , m_plugin(plugin)
    , m_config(config)
{
}

DeviceSession::~DeviceSession()
{
    if (!m_thread || !m_thread->isRunning()) {
        return;
    }

    requestStop();
    if (!m_thread->wait(3'000)) {
        // 不使用 terminate()。线程保留自己的 stopped -> quit -> deleteLater 链，
        // 同时断开对即将销毁的 Session 的回调，避免悬空访问。
        QObject::disconnect(m_thread, nullptr, this, nullptr);
        if (m_worker) {
            QObject::disconnect(m_worker, nullptr, this, nullptr);
        }
    }
}

bool DeviceSession::start()
{
    if (isRunning()) {
        reportLifecycleError(QStringLiteral("设备通信线程已经在运行"), true);
        return false;
    }
    if (!m_plugin) {
        reportLifecycleError(QStringLiteral("协议插件为空，无法创建设备 Worker"), false);
        return false;
    }

    auto *worker = m_plugin->createWorker(m_config);
    if (!worker) {
        reportLifecycleError(QStringLiteral("协议插件创建设备 Worker 失败"), false);
        return false;
    }
    if (worker->parent()) {
        reportLifecycleError(QStringLiteral("插件返回的 Worker 不能带 QObject parent"), false);
        worker->deleteLater();
        return false;
    }

    auto *thread = new QThread;
    thread->setObjectName(QStringLiteral("device-%1-communication").arg(m_config.id));
    m_thread = thread;
    m_worker = worker;

    // 先建立完整生命周期和代理信号，再移动并启动线程。
    connect(thread, &QThread::started,
            worker, &AbstractDeviceWorker::start,
            Qt::QueuedConnection);
    connect(this, &DeviceSession::stopWorkerRequested,
            worker, &AbstractDeviceWorker::stop,
            Qt::QueuedConnection);
    connect(this, &DeviceSession::writeWorkerRequested,
            worker, &AbstractDeviceWorker::writeValue,
            Qt::QueuedConnection);

    connect(worker, &AbstractDeviceWorker::stateChanged,
            this, &DeviceSession::stateChanged,
            Qt::QueuedConnection);
    connect(worker, &AbstractDeviceWorker::samplesReady,
            this, &DeviceSession::samplesReady,
            Qt::QueuedConnection);
    connect(worker, &AbstractDeviceWorker::writeFinished,
            this, &DeviceSession::writeFinished,
            Qt::QueuedConnection);
    connect(worker, &AbstractDeviceWorker::communicationError,
            this, &DeviceSession::communicationError,
            Qt::QueuedConnection);
    connect(worker, &AbstractDeviceWorker::transactionLogged,
            this, &DeviceSession::transactionLogged,
            Qt::QueuedConnection);

    connect(worker, &AbstractDeviceWorker::stopped,
            thread, &QThread::quit,
            Qt::DirectConnection);
    connect(thread, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this,
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

bool DeviceSession::stopAndWait(int timeoutMs)
{
    if (!m_thread || !m_thread->isRunning()) {
        return true;
    }
    if (QThread::currentThread() == m_thread) {
        reportLifecycleError(QStringLiteral("不能在设备通信线程内等待自身退出"), false);
        return false;
    }

    // 使用局部事件循环等待，使通信线程退出前已经投递到 Session 的
    // samples/state 信号仍能转发给数据线程；排除用户输入避免重入命令。
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(m_thread, &QThread::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    requestStop();
    timeoutTimer.start(qMax(0, timeoutMs));
    if (m_thread && m_thread->isRunning()) {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    // QThread::finished 与 Worker 的最后一批 queued 信号面向不同接收者，
    // 不能依赖二者的跨接收者派发顺序；返回前显式派发本 Session 的元调用。
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    if (!m_thread || !m_thread->isRunning()) {
        return true;
    }
    reportLifecycleError(
        QStringLiteral("设备通信线程未在 %1 ms 内退出").arg(timeoutMs),
        true);
    return false;
}

bool DeviceSession::isRunning() const
{
    return m_thread && m_thread->isRunning();
}

void DeviceSession::requestStop()
{
    if (m_worker) {
        emit stopWorkerRequested();
    }
}

void DeviceSession::writeValue(const WriteRequest &request)
{
    if (!isRunning() || !m_worker) {
        emit writeFinished({request.requestId,
                            false,
                            QStringLiteral("设备通信线程未运行")});
        return;
    }
    emit writeWorkerRequested(request);
}

void DeviceSession::reportLifecycleError(const QString &message,
                                         bool recoverable)
{
    DeviceError error;
    error.deviceId = m_config.id;
    error.code = -1;
    error.message = message;
    error.category = DeviceErrorCategory::Lifecycle;
    error.recoverable = recoverable;
    emit communicationError(error);
}
