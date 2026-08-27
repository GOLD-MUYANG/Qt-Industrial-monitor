#include "ModbusTcpWorker.h"

#include "ModbusRegisterCodec.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QTimer>

#include <algorithm>
#include <memory>

using namespace industrial::protocol;

namespace
{

DeviceErrorCategory categoryForModbusError(QModbusDevice::Error error)
{
    switch (error)
    {
    case QModbusDevice::ConfigurationError:
        return DeviceErrorCategory::Configuration;
    case QModbusDevice::ConnectionError:
        return DeviceErrorCategory::Connection;
    case QModbusDevice::TimeoutError:
        return DeviceErrorCategory::Timeout;
    case QModbusDevice::ProtocolError:
        return DeviceErrorCategory::Protocol;
    case QModbusDevice::ReplyAbortedError:
    case QModbusDevice::UnknownError:
    case QModbusDevice::NoError:
        return DeviceErrorCategory::Connection;
    }
    return DeviceErrorCategory::Connection;
}

} // namespace

ModbusTcpWorker::ModbusTcpWorker(const DeviceConfig &config, QObject *parent)
    : AbstractDeviceWorker(parent), m_config(config)
{
}

void ModbusTcpWorker::start()
{
    if (m_client)
    {
        return;
    }

    m_stopping = false;
    m_stoppedEmitted = false;
    QString validationError;
    if (!validateConfig(&validationError))
    {
        emitState(ConnectionState::Faulted, validationError);
        emitCommunicationError(DeviceErrorCategory::Configuration, -1, validationError, 0, false);
        return;
    }

    // 网络对象和定时器只在 Worker 已归属的通信线程中创建。
    m_client = new QModbusTcpClient(this);
    // 周期采集
    m_pollTimer = new QTimer(this);
    // 断线重连一次
    m_reconnectTimer = new QTimer(this);
    m_pollTimer->setInterval(m_config.pollIntervalMs);
    m_reconnectTimer->setSingleShot(true);

    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, m_config.host);
    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_config.port);
    m_client->setTimeout(m_config.timeoutMs);
    m_client->setNumberOfRetries(m_config.protocolRetries);

    connect(m_pollTimer, &QTimer::timeout, this, &ModbusTcpWorker::readSnapshot);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ModbusTcpWorker::attemptConnection);
    connect(m_client, &QModbusDevice::errorOccurred, this,
            [this](QModbusDevice::Error error)
            {
                if (error == QModbusDevice::NoError || m_stopping)
                {
                    return;
                }
                emitCommunicationError(categoryForModbusError(error), static_cast<int>(error),
                                       m_client->errorString(), m_activeReadRequestId);
            });
    connect(m_client, &QModbusDevice::stateChanged, this,
            [this](QModbusDevice::State state)
            {
                if (state == QModbusDevice::ConnectedState)
                {
                    if (m_reconnectTimer)
                    {
                        m_reconnectTimer->stop();
                    }
                    emitState(ConnectionState::Online);
                    m_pollTimer->start();
                    readSnapshot();
                    return;
                }
                if (state == QModbusDevice::UnconnectedState)
                {
                    if (m_stopping)
                    {
                        finishStop();
                    }
                    else
                    {
                        scheduleReconnect(m_client->errorString());
                    }
                }
            });

    attemptConnection();
}

void ModbusTcpWorker::stop()
{
    if (m_stoppedEmitted)
    {
        return;
    }

    m_stopping = true;
    emitState(ConnectionState::Stopping);
    if (m_pollTimer)
    {
        m_pollTimer->stop();
    }
    if (m_reconnectTimer)
    {
        m_reconnectTimer->stop();
    }

    if (!m_client || m_client->state() == QModbusDevice::UnconnectedState)
    {
        finishStop();
        return;
    }
    m_client->disconnectDevice();
}

void ModbusTcpWorker::writeValue(const WriteRequest &request)
{
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
    {
        emit writeFinished({request.requestId, false, QStringLiteral("设备尚未连接")});
        return;
    }
    if (request.address != 10)
    {
        emit writeFinished({request.requestId, false, QStringLiteral("首版只允许写入地址 10")});
        return;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, request.address, 1);
    unit.setValue(0, request.rawValue);
    QModbusReply *reply = m_client->sendWriteRequest(unit, m_config.unitId);
    if (!reply)
    {
        const QString message = m_client->errorString();
        emit writeFinished({request.requestId, false, message});
        emitCommunicationError(categoryForModbusError(m_client->error()),
                               static_cast<int>(m_client->error()), message, request.requestId);
        return;
    }

    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    connect(reply, &QModbusReply::finished, this,
            [this, reply, request, elapsed]()
            {
                const bool success = reply->error() == QModbusDevice::NoError;
                const QString error = success ? QString() : reply->errorString();
                emit writeFinished({request.requestId, success, error});
                emit transactionLogged({m_config.id, request.requestId, 6, request.address, 1,
                                        elapsed->elapsed(), success, error, false});
                if (!success && !m_stopping)
                {
                    emitCommunicationError(categoryForModbusError(reply->error()),
                                           static_cast<int>(reply->error()), error,
                                           request.requestId);
                }
                reply->deleteLater();
            });
}

void ModbusTcpWorker::emitState(ConnectionState state, const QString &message)
{
    m_state = state;
    emit stateChanged({m_config.id, state, message});
}

void ModbusTcpWorker::emitCommunicationError(DeviceErrorCategory category,
                                             int code,
                                             const QString &message,
                                             quint64 requestId,
                                             bool recoverable)
{
    DeviceError error;
    error.deviceId = m_config.id;
    error.code = code;
    error.message = message;
    error.category = category;
    error.requestId = requestId;
    error.recoverable = recoverable;
    emit communicationError(error);
}

bool ModbusTcpWorker::validateConfig(QString *errorMessage) const
{
    const bool reconnectDelaysValid =
        !m_config.reconnectDelaysMs.isEmpty() &&
        std::all_of(m_config.reconnectDelaysMs.cbegin(), m_config.reconnectDelaysMs.cend(),
                    [](int delay) { return delay > 0; });
    const bool valid = !m_config.id.trimmed().isEmpty() && !m_config.host.trimmed().isEmpty() &&
                       m_config.port != 0 && m_config.unitId >= 1 && m_config.unitId <= 247 &&
                       m_config.timeoutMs >= 10 && m_config.protocolRetries >= 0 &&
                       m_config.pollIntervalMs > 0 && m_config.consecutiveFailureLimit > 0 &&
                       reconnectDelaysValid;
    if (!valid && errorMessage)
    {
        *errorMessage =
            QStringLiteral("设备 ID、端点、Unit ID、超时、轮询、失败阈值或重连延迟配置无效");
    }
    return valid;
}

void ModbusTcpWorker::attemptConnection()
{
    if (m_stopping || !m_client)
    {
        return;
    }
    if (m_client->state() != QModbusDevice::UnconnectedState)
    {
        if (m_client->state() != QModbusDevice::ConnectedState)
        {
            m_reconnectTimer->start(10);
        }
        return;
    }

    emitState(ConnectionState::Connecting);
    if (!m_client->connectDevice())
    {
        const QString message = m_client->errorString();
        emitCommunicationError(categoryForModbusError(m_client->error()),
                               static_cast<int>(m_client->error()), message);
        scheduleReconnect(message);
    }
}

void ModbusTcpWorker::scheduleReconnect(const QString &reason)
{
    if (m_stopping || !m_reconnectTimer || m_reconnectTimer->isActive())
    {
        return;
    }

    if (m_pollTimer)
    {
        m_pollTimer->stop();
    }
    const int lastIndex = m_config.reconnectDelaysMs.size() - 1;
    const int delayIndex = std::min(m_reconnectDelayIndex, lastIndex);
    const int delayMs = m_config.reconnectDelaysMs.at(delayIndex);
    if (m_reconnectDelayIndex < lastIndex)
    {
        ++m_reconnectDelayIndex;
    }

    const QString message = reason.isEmpty()
                                ? QStringLiteral("连接中断，%1 ms 后重连").arg(delayMs)
                                : QStringLiteral("%1；%2 ms 后重连").arg(reason).arg(delayMs);
    emitState(ConnectionState::Reconnecting, message);
    m_reconnectTimer->start(delayMs);

    if (m_client && m_client->state() != QModbusDevice::UnconnectedState)
    {
        m_client->disconnectDevice();
    }
}

void ModbusTcpWorker::handleReadFailure(DeviceErrorCategory category,
                                        int code,
                                        const QString &message,
                                        quint64 requestId)
{
    if (m_stopping)
    {
        return;
    }

    emitCommunicationError(category, code, message, requestId);
    ++m_consecutiveFailures;
    if (m_consecutiveFailures >= m_config.consecutiveFailureLimit)
    {
        scheduleReconnect(message);
    }
}

void ModbusTcpWorker::readSnapshot()
{
    if (m_stopping || !m_client || m_client->state() != QModbusDevice::ConnectedState)
    {
        return;
    }
    if (m_readInFlight)
    {
        emit transactionLogged({m_config.id, m_activeReadRequestId, 3, 0, 5, 0, false,
                                QStringLiteral("上一读取请求仍在途，跳过本次轮询"), true});
        return;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 5);
    const quint64 requestId = ++m_readRequestId;
    QModbusReply *reply = m_client->sendReadRequest(unit, m_config.unitId);
    if (!reply)
    {
        const QString message = m_client->errorString();
        handleReadFailure(categoryForModbusError(m_client->error()),
                          static_cast<int>(m_client->error()), message, requestId);
        return;
    }

    m_readInFlight = true;
    m_activeReadRequestId = requestId;
    m_readReply = reply;
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    connect(reply, &QModbusReply::finished, this,
            [this, reply, requestId, elapsed]()
            {
                m_readInFlight = false;
                m_activeReadRequestId = 0;
                m_readReply.clear();

                bool success = reply->error() == QModbusDevice::NoError;
                QString error = success ? QString() : reply->errorString();
                DeviceErrorCategory errorCategory = categoryForModbusError(reply->error());
                int errorCode = static_cast<int>(reply->error());
                SampleBatch decodedSamples;

                if (success)
                {
                    const auto decoded = ModbusRegisterCodec::decodeSnapshot(
                        reply->result().values(), m_config.id, QDateTime::currentDateTimeUtc(),
                        ++m_sequence);
                    success = decoded.success;
                    error = decoded.errorMessage;
                    decodedSamples = decoded.samples;
                    if (!success)
                    {
                        errorCategory = DeviceErrorCategory::Data;
                        errorCode = -2;
                    }
                }

                if (success)
                {
                    m_consecutiveFailures = 0;
                    m_reconnectDelayIndex = 0;
                    emit samplesReady(decodedSamples);
                }
                else
                {
                    handleReadFailure(errorCategory, errorCode, error, requestId);
                }
                emit transactionLogged(
                    {m_config.id, requestId, 3, 0, 5, elapsed->elapsed(), success, error, false});
                reply->deleteLater();
            });
}

void ModbusTcpWorker::finishStop()
{
    if (m_stoppedEmitted)
    {
        return;
    }

    m_stoppedEmitted = true;
    m_readInFlight = false;
    m_readReply.clear();
    if (m_client)
    {
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_pollTimer = nullptr;
    m_reconnectTimer = nullptr;
    emitState(ConnectionState::Stopped);
    emit stopped();
}
