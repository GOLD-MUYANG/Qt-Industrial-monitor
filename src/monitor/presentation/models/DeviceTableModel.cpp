#include "DeviceTableModel.h"

#include <QBrush>
#include <QColor>

using namespace industrial::protocol;

DeviceTableModel::DeviceTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int DeviceTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_hasDevice ? 0 : 1;
}

int DeviceTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DeviceTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() != 0 || !m_hasDevice) {
        return {};
    }
    if (role == Qt::ForegroundRole && index.column() == StateColumn) {
        return QBrush(stateColor(m_state.connectionState));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (index.column()) {
    case IdColumn:
        return m_device.id;
    case ProtocolColumn:
        return m_protocolNames.value(m_device.protocolKey,
                                     m_device.protocolKey);
    case EndpointColumn:
        return QStringLiteral("%1:%2").arg(m_device.host).arg(m_device.port);
    case UnitIdColumn:
        return m_device.unitId;
    case PollColumn:
        return QStringLiteral("%1 ms").arg(m_device.pollIntervalMs);
    case LastCommunicationColumn:
        return m_lastCommunicationUtc.isValid()
            ? m_lastCommunicationUtc.toLocalTime().toString(
                  QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("尚无数据");
    case StateColumn:
        return stateText(m_state.connectionState);
    default:
        return {};
    }
}

void DeviceTableModel::addProtocol(const ProtocolDescriptor &descriptor)
{
    if (descriptor.key.isEmpty()) {
        return;
    }
    m_protocolNames.insert(descriptor.key, descriptor.displayName);
    if (m_hasDevice && descriptor.key == m_device.protocolKey) {
        emit dataChanged(index(0, ProtocolColumn), index(0, ProtocolColumn));
    }
}

QVariant DeviceTableModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= ColumnCount) {
        return {};
    }
    static const QStringList headers = {
        QStringLiteral("设备 ID"),
        QStringLiteral("协议"),
        QStringLiteral("端点"),
        QStringLiteral("Unit ID"),
        QStringLiteral("轮询"),
        QStringLiteral("最近通信"),
        QStringLiteral("连接状态"),
    };
    return headers.at(section);
}

void DeviceTableModel::setDevice(const DeviceConfig &config)
{
    beginResetModel();
    m_device = config;
    m_hasDevice = !config.id.isEmpty();
    if (m_state.deviceId != config.id) {
        m_state = {};
        m_state.deviceId = config.id;
    }
    endResetModel();
}

void DeviceTableModel::setState(const DeviceState &state)
{
    if (!m_hasDevice || state.deviceId != m_device.id) {
        return;
    }
    m_state = state;
    emit dataChanged(index(0, StateColumn), index(0, StateColumn));
}

void DeviceTableModel::setLastCommunicationTime(const QDateTime &timestampUtc)
{
    if (!m_hasDevice || !timestampUtc.isValid()) {
        return;
    }
    m_lastCommunicationUtc = timestampUtc;
    emit dataChanged(index(0, LastCommunicationColumn),
                     index(0, LastCommunicationColumn));
}

QString DeviceTableModel::stateText(ConnectionState state)
{
    switch (state) {
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

QColor DeviceTableModel::stateColor(ConnectionState state)
{
    if (state == ConnectionState::Online) {
        return QColor(QStringLiteral("#137333"));
    }
    if (state == ConnectionState::Reconnecting
        || state == ConnectionState::Connecting) {
        return QColor(QStringLiteral("#9A6700"));
    }
    if (state == ConnectionState::Faulted) {
        return QColor(QStringLiteral("#B42318"));
    }
    return QColor(QStringLiteral("#475569"));
}
