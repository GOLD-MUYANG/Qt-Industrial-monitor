#include "AlarmTableModel.h"

#include <QBrush>
#include <QColor>

using namespace industrial::monitor;

AlarmTableModel::AlarmTableModel(Mode mode, QObject *parent)
    : QAbstractTableModel(parent)
    , m_mode(mode)
{
}

int AlarmTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : visibleRecords().size();
}

int AlarmTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant AlarmTableModel::data(const QModelIndex &index, int role) const
{
    const auto records = visibleRecords();
    if (!index.isValid() || index.row() < 0 || index.row() >= records.size()) {
        return {};
    }
    const auto &alarm = records.at(index.row());
    if (role == Qt::ForegroundRole && index.column() == SeverityColumn) {
        return QBrush(severityColor(alarm.severity));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (index.column()) {
    case TimeColumn:
        return alarm.activatedAtUtc.toLocalTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case DeviceColumn:
        return alarm.deviceId;
    case TagColumn:
        return alarm.tagId.isEmpty() ? QStringLiteral("通信") : alarm.tagId;
    case MessageColumn:
        return alarm.message;
    case TriggerColumn:
        return alarm.kind == AlarmKind::Communication
            ? QStringLiteral("—")
            : QString::number(alarm.triggerValue, 'f', 2);
    case SeverityColumn:
        return severityText(alarm.severity);
    case StateColumn:
        return stateText(alarm.state);
    default:
        return {};
    }
}

QVariant AlarmTableModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= ColumnCount) {
        return {};
    }
    static const QStringList headers = {
        QStringLiteral("激活时间"),
        QStringLiteral("设备"),
        QStringLiteral("测点/类型"),
        QStringLiteral("消息"),
        QStringLiteral("触发值"),
        QStringLiteral("等级"),
        QStringLiteral("状态"),
    };
    return headers.at(section);
}

QString AlarmTableModel::alarmIdAt(int row) const
{
    return alarmAt(row).id;
}

AlarmRecord AlarmTableModel::alarmAt(int row) const
{
    const auto records = visibleRecords();
    return row >= 0 && row < records.size() ? records.at(row) : AlarmRecord{};
}

void AlarmTableModel::upsertAlarm(const AlarmRecord &alarm)
{
    if (alarm.id.isEmpty()) {
        return;
    }
    beginResetModel();
    bool updated = false;
    for (auto &record : m_records) {
        if (record.id == alarm.id) {
            record = alarm;
            updated = true;
            break;
        }
    }
    if (!updated) {
        m_records.prepend(alarm);
    }
    endResetModel();
}

bool AlarmTableModel::isActive(AlarmState state)
{
    return state == AlarmState::ActiveUnacknowledged
        || state == AlarmState::ActiveAcknowledged;
}

QString AlarmTableModel::severityText(AlarmSeverity severity)
{
    return severity == AlarmSeverity::Critical
        ? QStringLiteral("严重 (Critical)")
        : QStringLiteral("警告 (Warning)");
}

QString AlarmTableModel::stateText(AlarmState state)
{
    switch (state) {
    case AlarmState::Pending:
        return QStringLiteral("等待连续样本 (Pending)");
    case AlarmState::ActiveUnacknowledged:
        return QStringLiteral("活动 / 未确认");
    case AlarmState::ActiveAcknowledged:
        return QStringLiteral("活动 / 已确认");
    case AlarmState::RecoveredUnacknowledged:
        return QStringLiteral("已恢复 / 未确认");
    case AlarmState::RecoveredAcknowledged:
        return QStringLiteral("已恢复 / 已确认");
    }
    return QStringLiteral("未知");
}

QColor AlarmTableModel::severityColor(AlarmSeverity severity)
{
    return severity == AlarmSeverity::Critical
        ? QColor(QStringLiteral("#B42318"))
        : QColor(QStringLiteral("#9A6700"));
}

QList<AlarmRecord> AlarmTableModel::visibleRecords() const
{
    if (m_mode == Mode::History) {
        return m_records;
    }
    QList<AlarmRecord> active;
    for (const auto &record : m_records) {
        if (isActive(record.state)) {
            active.append(record);
        }
    }
    return active;
}
