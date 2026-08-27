#include "QmlDeviceViewModel.h"

using namespace industrial::protocol;

QmlDeviceViewModel::QmlDeviceViewModel(QObject *parent)
    : QObject(parent)
{
}

QString QmlDeviceViewModel::deviceId() const { return m_device.id; }
QString QmlDeviceViewModel::deviceName() const { return m_device.name; }
QString QmlDeviceViewModel::protocolKey() const { return m_device.protocolKey; }

QString QmlDeviceViewModel::protocolName() const
{
    return m_protocolNames.value(m_device.protocolKey, m_device.protocolKey);
}

QString QmlDeviceViewModel::host() const { return m_device.host; }
int QmlDeviceViewModel::port() const { return m_device.port; }
int QmlDeviceViewModel::unitId() const { return m_device.unitId; }
int QmlDeviceViewModel::pollIntervalMs() const { return m_device.pollIntervalMs; }
int QmlDeviceViewModel::timeoutMs() const { return m_device.timeoutMs; }
bool QmlDeviceViewModel::enabled() const { return m_device.enabled; }

int QmlDeviceViewModel::connectionState() const
{
    return static_cast<int>(m_state.connectionState);
}

QString QmlDeviceViewModel::connectionStateText() const
{
    return stateText(m_state.connectionState);
}

QString QmlDeviceViewModel::connectionMessage() const
{
    return m_state.message;
}

QString QmlDeviceViewModel::lastCommunicationText() const
{
    return m_lastCommunicationUtc.isValid()
        ? m_lastCommunicationUtc.toLocalTime().toString(
              QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("尚无数据");
}

void QmlDeviceViewModel::setDevice(const DeviceConfig &config)
{
    const bool identityChanged = m_device.id != config.id;
    m_device = config;
    if (identityChanged)
    {
        m_state = {};
        m_state.deviceId = config.id;
        m_lastCommunicationUtc = {};
        emit connectionChanged();
        emit lastCommunicationChanged();
    }
    emit deviceChanged();
    emit protocolNameChanged();
}

void QmlDeviceViewModel::setState(const DeviceState &state)
{
    if (m_device.id.isEmpty() || state.deviceId != m_device.id)
    {
        return;
    }
    m_state = state;
    emit connectionChanged();
}

void QmlDeviceViewModel::setLastCommunicationTime(const QDateTime &timestampUtc)
{
    if (!timestampUtc.isValid())
    {
        return;
    }
    m_lastCommunicationUtc = timestampUtc;
    emit lastCommunicationChanged();
}

void QmlDeviceViewModel::setProtocolDisplayName(const QString &protocolKey,
                                                const QString &displayName)
{
    if (protocolKey.isEmpty())
    {
        return;
    }
    m_protocolNames.insert(protocolKey,
                           displayName.isEmpty() ? protocolKey : displayName);
    if (protocolKey == m_device.protocolKey)
    {
        emit protocolNameChanged();
    }
}

QString QmlDeviceViewModel::stateText(ConnectionState state)
{
    switch (state)
    {
    case ConnectionState::Stopped:
        return QStringLiteral("已停止 (Stopped)");
    case ConnectionState::Connecting:
        return QStringLiteral("连接中 (Connecting)");
    case ConnectionState::Online:
        return QStringLiteral("在线 (Online)");
    case ConnectionState::Reconnecting:
        return QStringLiteral("重连中 (Reconnecting)");
    case ConnectionState::Stopping:
        return QStringLiteral("停止中 (Stopping)");
    case ConnectionState::Faulted:
        return QStringLiteral("故障 (Faulted)");
    }
    return QStringLiteral("未知");
}
