#include "ApplicationController.h"

#include "DataPipeline.h"
#include "DeviceSession.h"
#include "HistoryWorker.h"
#include "StorageTypes.h"
#include "StorageWorker.h"
#include "VisionSession.h"

#include <QMetaObject>

using namespace industrial::monitor;
using namespace industrial::monitor::vision;
using namespace industrial::protocol;

ApplicationController::ApplicationController(const QString &pluginDirectory,
                                             const QString &databasePath,
                                             QObject *parent)
    : QObject(parent), m_pluginDirectory(pluginDirectory), m_databasePath(databasePath)
{
    m_dataThread.setObjectName(QStringLiteral("data-pipeline"));
    m_storageThread.setObjectName(QStringLiteral("sqlite-writer"));
    m_historyThread.setObjectName(QStringLiteral("sqlite-history-reader"));
}

ApplicationController::~ApplicationController()
{
    shutdown();
}

bool ApplicationController::start()
{
    if (m_started)
    {
        return false;
    }
    registerProtocolMetaTypes();
    registerAlarmMetaTypes();
    registerStorageMetaTypes();
    registerVisionMetaTypes();

    m_pluginManager.scan(m_pluginDirectory);
    if (!m_pluginManager.errors().isEmpty())
    {
        const auto &error = m_pluginManager.errors().constFirst();
        emit fatalError(QStringLiteral("插件加载失败：%1：%2").arg(error.filePath, error.message));
        return false;
    }
    m_plugin = m_pluginManager.plugin(QStringLiteral("modbus-tcp"));
    if (!m_plugin)
    {
        emit fatalError(QStringLiteral("未发现 modbus-tcp 动态插件"));
        return false;
    }
    emit protocolAvailable(m_plugin->descriptor());

    setupVisionSession();
    if (!m_visionSession->start())
    {
        emit visionError(QStringLiteral("启动视觉线程失败"));
        m_visionSession.reset();
    }

    setupDataThread();
    setupStorageThread();
    setupHistoryThread();
    m_dataThread.start();
    m_storageThread.start();
    m_historyThread.start();
    m_started = true;
    emit startStorageRequested(m_databasePath);
    return true;
}

/**
停止数据生产
    ↓
排空数据处理队列
    ↓
刷盘并关闭写数据库
    ↓
关闭读数据库
*/
bool ApplicationController::shutdown(int timeoutMs)
{
    // 防止重复关闭
    if (!m_started && !isRunning())
    {
        return true;
    }

    bool clean = true;
    if (m_visionSession)
    {
        if (!m_visionSession->shutdown(timeoutMs))
        {
            clean = false;
            emit visionError(QStringLiteral("视觉线程未能在 %1 ms 内停止").arg(timeoutMs));
        }
        else
        {
            m_visionSession.reset();
        }
    }
    if (m_session)
    {
        if (!stopDeviceSession(timeoutMs))
        {
            clean = false;
            // Session 析构会再请求一次停止；插件被 PreventUnloadHint 保留，
            // 超时 Worker 即使稍后退出也不会执行已卸载的代码。
            m_session.reset();
        }
    }

    if (m_dataThread.isRunning())
    {
        if (m_pipeline)
        {
            /**
            这就是一个空的lambda，就是最后投递一个任务，阻塞当前的主线程，等到数据线程处理了这个空任务的时候
            就可以认为所有数据都被处理了，主线程不再阻塞，继续执行后面的。
            */
            const bool drained = QMetaObject::invokeMethod(
                m_pipeline, []() {}, Qt::BlockingQueuedConnection);
            clean = drained && clean;
        }
        m_dataThread.quit();
        clean = m_dataThread.wait(static_cast<unsigned long>(timeoutMs)) && clean;
    }

    if (m_storageThread.isRunning() && m_storage)
    {
        // 执行m_storage这个类里面的stop函数
        const bool invoked =
            QMetaObject::invokeMethod(m_storage, "stop", Qt::BlockingQueuedConnection);
        clean = invoked && clean;
        m_storageThread.quit();
        clean = m_storageThread.wait(static_cast<unsigned long>(timeoutMs)) && clean;
    }

    if (m_historyThread.isRunning() && m_history)
    {
        const bool invoked =
            QMetaObject::invokeMethod(m_history, "stop", Qt::BlockingQueuedConnection);
        clean = invoked && clean;
        m_historyThread.quit();
        clean = m_historyThread.wait(static_cast<unsigned long>(timeoutMs)) && clean;
    }

    m_started = false;
    m_initialized = false;
    return clean;
}

bool ApplicationController::isRunning() const
{
    return (m_visionSession && m_visionSession->isRunning()) ||
           (m_session && m_session->isRunning()) || m_dataThread.isRunning() ||
           m_storageThread.isRunning() || m_historyThread.isRunning();
}

void ApplicationController::openVisionVideo(const QString &path)
{
    if (m_started && m_visionSession) {
        m_visionSession->openVideo(path);
    }
}

void ApplicationController::playVisionVideo()
{
    if (m_started && m_visionSession) {
        m_visionSession->play();
    }
}

void ApplicationController::pauseVisionVideo()
{
    if (m_started && m_visionSession) {
        m_visionSession->pause();
    }
}

void ApplicationController::stopVisionVideo()
{
    if (m_started && m_visionSession) {
        m_visionSession->stop();
    }
}

void ApplicationController::connectDevice()
{
    if (!m_started || !m_initialized || m_session || !m_plugin)
    {
        return;
    }
    if (!m_device.enabled)
    {
        emit storageStatusChanged(QStringLiteral("设备已停用，未建立连接"), false);
        return;
    }

    m_session = std::make_unique<DeviceSession>(m_plugin, m_device);
    connect(m_session.get(), &DeviceSession::samplesReady, m_pipeline,
            &DataPipeline::processSamples, Qt::QueuedConnection);
    connect(m_session.get(), &DeviceSession::stateChanged, m_pipeline,
            &DataPipeline::handleDeviceState, Qt::QueuedConnection);
    connect(m_session.get(), &DeviceSession::stateChanged, this,
            &ApplicationController::deviceStateChanged, Qt::QueuedConnection);
    connect(m_session.get(), &DeviceSession::communicationError, this,
            &ApplicationController::reportCommunicationError, Qt::QueuedConnection);
    connect(m_session.get(), &DeviceSession::writeFinished, this,
            &ApplicationController::writeFinished, Qt::QueuedConnection);
    if (!m_session->start())
    {
        emit fatalError(QStringLiteral("启动设备通信线程失败"));
        m_session.reset();
    }
}

void ApplicationController::writeTargetSpeed(quint16 targetSpeed)
{
    WriteRequest request;
    request.deviceId = m_device.id;
    request.tagId = QStringLiteral("target-speed");
    request.address = 10;
    request.rawValue = targetSpeed;
    request.requestId = m_nextWriteRequestId++;
    if (!m_session || !m_session->isRunning())
    {
        emit writeFinished({request.requestId, false, QStringLiteral("设备尚未连接")});
        return;
    }
    m_session->writeValue(request);
}

bool ApplicationController::disconnectDevice()
{
    return stopDeviceSession(3'000);
}

bool ApplicationController::stopDeviceSession(int timeoutMs)
{
    if (!m_session)
    {
        return true;
    }
    emit deviceStateChanged(
        {m_device.id, ConnectionState::Stopping, QStringLiteral("正在停止设备会话")});
    // stopAndWait启动一个局部的事件循环，让已经发出的数据和状态信息能够被处理。
    if (!m_session->stopAndWait(timeoutMs))
    {
        emit fatalError(QStringLiteral("设备通信线程未能在 %1 ms 内停止").arg(timeoutMs));
        return false;
    }
    m_session.reset();
    if (m_pipeline && m_dataThread.isRunning())
    {
        const bool reset =
            QMetaObject::invokeMethod(m_pipeline, "resetDeviceSession",
                                      Qt::BlockingQueuedConnection, Q_ARG(QString, m_device.id));
        if (!reset)
        {
            emit fatalError(QStringLiteral("无法重置数据会话序号基线"));
            return false;
        }
    }
    emit deviceStateChanged(
        {m_device.id, ConnectionState::Stopped, QStringLiteral("设备会话已停止")});
    return true;
}

void ApplicationController::applyDeviceConfig(const DeviceConfig &config)
{
    if (!m_started || !m_initialized)
    {
        return;
    }
    if (config.id != m_device.id || config.host.trimmed().isEmpty() ||
        !m_pluginManager.plugin(config.protocolKey) || config.port == 0 || config.unitId < 1 ||
        config.unitId > 247 || config.pollIntervalMs < 50 || config.timeoutMs < 1)
    {
        emit storageStatusChanged(QStringLiteral("设备配置无效"), false);
        return;
    }
    m_restartAfterSave = config.enabled && m_session && m_session->isRunning();
    if (m_session && !disconnectDevice())
    {
        m_restartAfterSave = false;
        return;
    }
    emit saveDeviceRequested(config);
}

void ApplicationController::acknowledgeAlarm(const QString &alarmId, const QString &note)
{
    if (!alarmId.isEmpty())
    {
        emit acknowledgeAlarmRequested(alarmId, note);
    }
}

void ApplicationController::setupDataThread()
{
    auto *pipeline = new DataPipeline;
    m_pipeline = pipeline;
    pipeline->moveToThread(&m_dataThread);
    connect(&m_dataThread, &QThread::finished, pipeline, &QObject::deleteLater);
    connect(this, &ApplicationController::configureAlarmRulesRequested, pipeline,
            &DataPipeline::setAlarmRules, Qt::QueuedConnection);
    connect(this, &ApplicationController::acknowledgeAlarmRequested, pipeline,
            &DataPipeline::acknowledgeAlarm, Qt::QueuedConnection);
    connect(pipeline, &DataPipeline::snapshotsReady, this, &ApplicationController::snapshotsReady,
            Qt::QueuedConnection);
    connect(pipeline, &DataPipeline::alarmChanged, this, &ApplicationController::alarmChanged,
            Qt::QueuedConnection);
    connect(
        pipeline, &DataPipeline::pipelineError, this,
        [this](const DeviceError &error)
        { emit storageStatusChanged(QStringLiteral("数据处理：%1").arg(error.message), false); },
        Qt::QueuedConnection);
}

void ApplicationController::setupStorageThread()
{
    auto *storage = new StorageWorker;
    m_storage = storage;
    storage->moveToThread(&m_storageThread);
    connect(&m_storageThread, &QThread::finished, storage, &QObject::deleteLater);
    connect(this, &ApplicationController::startStorageRequested, storage, &StorageWorker::start,
            Qt::QueuedConnection);
    connect(this, &ApplicationController::saveDeviceRequested, storage, &StorageWorker::saveDevice,
            Qt::QueuedConnection);
    connect(
        storage, &StorageWorker::ready, this,
        [this](const DeviceConfig &device, const AlarmRuleList &rules, quintptr)
        {
            m_device = device;
            emit deviceConfigChanged(device);
            emit configureAlarmRulesRequested(rules);
            emit storageStatusChanged(QStringLiteral("SQLite 写连接在线"), true);
            emit startHistoryRequested(m_databasePath);
        },
        Qt::QueuedConnection);
    connect(
        storage, &StorageWorker::deviceSaved, this,
        [this](const DeviceConfig &device)
        {
            m_device = device;
            m_plugin = m_pluginManager.plugin(device.protocolKey);
            emit deviceConfigChanged(device);
            emit storageStatusChanged(QStringLiteral("设备配置已保存"), true);
            if (m_restartAfterSave)
            {
                m_restartAfterSave = false;
                connectDevice();
            }
        },
        Qt::QueuedConnection);
    connect(
        storage, &StorageWorker::storageError, this,
        [this](const QString &message)
        {
            emit storageStatusChanged(message, false);
            if (!m_initialized)
            {
                emit fatalError(message);
            }
        },
        Qt::QueuedConnection);
    connect(m_pipeline, &DataPipeline::alarmChanged, storage, &StorageWorker::enqueueAlarm,
            Qt::QueuedConnection);
    connect(m_pipeline, &DataPipeline::validatedSamplesReady, storage,
            &StorageWorker::enqueueSamples, Qt::QueuedConnection);
}

void ApplicationController::setupHistoryThread()
{
    auto *history = new HistoryWorker;
    m_history = history;
    history->moveToThread(&m_historyThread);
    connect(&m_historyThread, &QThread::finished, history, &QObject::deleteLater);
    connect(this, &ApplicationController::startHistoryRequested, history, &HistoryWorker::start,
            Qt::QueuedConnection);
    connect(
        history, &HistoryWorker::ready, this,
        [this](quintptr)
        {
            if (!m_initialized)
            {
                m_initialized = true;
                emit initialized(m_device);
            }
            emit storageStatusChanged(QStringLiteral("SQLite 读写连接在线"), true);
        },
        Qt::QueuedConnection);
    connect(
        history, &HistoryWorker::historyError, this,
        [this](const QString &message)
        {
            emit storageStatusChanged(message, false);
            if (!m_initialized)
            {
                emit fatalError(message);
            }
        },
        Qt::QueuedConnection);
}

void ApplicationController::setupVisionSession()
{
    m_visionSession = std::make_unique<VisionSession>();
    connect(m_visionSession.get(), &VisionSession::sourceOpened,
            this, &ApplicationController::visionSourceOpened);
    connect(m_visionSession.get(), &VisionSession::frameReady,
            this, &ApplicationController::visionFrameReady);
    connect(m_visionSession.get(), &VisionSession::stateChanged,
            this, &ApplicationController::visionStateChanged);
    connect(m_visionSession.get(), &VisionSession::videoError,
            this, &ApplicationController::visionError);
}

void ApplicationController::reportCommunicationError(const DeviceError &error)
{
    emit storageStatusChanged(QStringLiteral("通信：%1").arg(error.message), false);
}
