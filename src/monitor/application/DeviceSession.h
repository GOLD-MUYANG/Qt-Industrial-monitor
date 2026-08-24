#pragma once

#include <QObject>
#include <QPointer>

#include <industrial/protocol/IProtocolPlugin.h>

class QThread;

class DeviceSession final : public QObject
{
    Q_OBJECT

public:
    explicit DeviceSession(
        industrial::protocol::IProtocolPlugin *plugin,
        const industrial::protocol::DeviceConfig &config,
        QObject *parent = nullptr);
    ~DeviceSession() override;

    bool start();
    bool stopAndWait(int timeoutMs);
    bool isRunning() const;

public slots:
    void requestStop();
    void writeValue(const industrial::protocol::WriteRequest &request);

signals:
    void stateChanged(const industrial::protocol::DeviceState &state);
    void samplesReady(const industrial::protocol::SampleBatch &samples);
    void writeFinished(const industrial::protocol::WriteResult &result);
    void communicationError(const industrial::protocol::DeviceError &error);
    void transactionLogged(const industrial::protocol::TransactionLog &log);
    void stopped();

    // 这三个命令信号只用于跨线程投递，不向调用方暴露 Worker 指针。
    void stopWorkerRequested();
    void writeWorkerRequested(
        const industrial::protocol::WriteRequest &request);

private:
    void reportLifecycleError(const QString &message, bool recoverable);

    industrial::protocol::IProtocolPlugin *m_plugin;
    industrial::protocol::DeviceConfig m_config;
    QPointer<QThread> m_thread;
    QPointer<industrial::protocol::AbstractDeviceWorker> m_worker;
};
