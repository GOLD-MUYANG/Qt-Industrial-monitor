#pragma once

#include <industrial/protocol/AbstractDeviceWorker.h>

#include <QPointer>

class QModbusReply;
class QModbusTcpClient;
class QTimer;

class ModbusTcpWorker final
    : public industrial::protocol::AbstractDeviceWorker
{
    Q_OBJECT

public:
    explicit ModbusTcpWorker(
        const industrial::protocol::DeviceConfig &config,
        QObject *parent = nullptr);

public slots:
    void start() override;
    void stop() override;
    void writeValue(
        const industrial::protocol::WriteRequest &request) override;

private:
    void emitState(industrial::protocol::ConnectionState state,
                   const QString &message = {});
    void emitCommunicationError(
        industrial::protocol::DeviceErrorCategory category,
        int code,
        const QString &message,
        quint64 requestId = 0,
        bool recoverable = true);
    bool validateConfig(QString *errorMessage) const;
    void attemptConnection();
    void scheduleReconnect(const QString &reason);
    void handleReadFailure(
        industrial::protocol::DeviceErrorCategory category,
        int code,
        const QString &message,
        quint64 requestId);
    void readSnapshot();
    void finishStop();

    industrial::protocol::DeviceConfig m_config;
    QModbusTcpClient *m_client = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QPointer<QModbusReply> m_readReply;
    quint64 m_sequence = 0;
    quint64 m_readRequestId = 0;
    quint64 m_activeReadRequestId = 0;
    int m_consecutiveFailures = 0;
    int m_reconnectDelayIndex = 0;
    industrial::protocol::ConnectionState m_state =
        industrial::protocol::ConnectionState::Stopped;
    bool m_readInFlight = false;
    bool m_stopping = false;
    bool m_stoppedEmitted = false;
};
