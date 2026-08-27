#include "QmlAlarmModel.h"

using namespace industrial::monitor;

QmlAlarmModel::QmlAlarmModel(Mode mode, QObject *parent)
    : QAbstractListModel(parent), m_mode(mode)
{
}

int QmlAlarmModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : visibleRecords().size();
}

QVariant QmlAlarmModel::data(const QModelIndex &index, int role) const
{
    const auto records = visibleRecords();
    if (!index.isValid() || index.row() < 0 || index.row() >= records.size())
    {
        return {};
    }
    const auto &alarm = records.at(index.row());
    switch (role)
    {
    case AlarmIdRole:
        return alarm.id;
    case ActivatedAtTextRole:
        return alarm.activatedAtUtc.toLocalTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case DeviceIdRole:
        return alarm.deviceId;
    case TagTextRole:
        return alarm.tagId.isEmpty() ? QStringLiteral("通信") : alarm.tagId;
    case MessageRole:
        return alarm.message;
    case TriggerValueRole:
        return alarm.kind == AlarmKind::Communication
            ? QVariant{}
            : QVariant::fromValue(alarm.triggerValue);
    case SeverityRole:
        return static_cast<int>(alarm.severity);
    case SeverityTextRole:
        return severityText(alarm.severity);
    case StateRole:
        return static_cast<int>(alarm.state);
    case StateTextRole:
        return stateText(alarm.state);
    case ActiveRole:
        return isActive(alarm.state);
    case AcknowledgeableRole:
        return isAcknowledgeable(alarm.state);
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlAlarmModel::roleNames() const
{
    return {
        {AlarmIdRole, "alarmId"},
        {ActivatedAtTextRole, "activatedAtText"},
        {DeviceIdRole, "deviceId"},
        {TagTextRole, "tagText"},
        {MessageRole, "message"},
        {TriggerValueRole, "triggerValue"},
        {SeverityRole, "severity"},
        {SeverityTextRole, "severityText"},
        {StateRole, "state"},
        {StateTextRole, "stateText"},
        {ActiveRole, "active"},
        {AcknowledgeableRole, "acknowledgeable"},
    };
}

QString QmlAlarmModel::alarmIdAt(int row) const
{
    const auto records = visibleRecords();
    return row >= 0 && row < records.size() ? records.at(row).id : QString{};
}

bool QmlAlarmModel::isAcknowledgeableId(const QString &alarmId) const
{
    for (const auto &record : m_records)
    {
        if (record.id == alarmId)
        {
            return isAcknowledgeable(record.state);
        }
    }
    return false;
}

void QmlAlarmModel::upsertAlarm(const AlarmRecord &alarm)
{
    if (alarm.id.isEmpty())
    {
        return;
    }

    beginResetModel();
    bool updated = false;
    for (auto &record : m_records)
    {
        if (record.id == alarm.id)
        {
            record = alarm;
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        m_records.prepend(alarm);
    }
    endResetModel();
}

bool QmlAlarmModel::isActive(AlarmState state)
{
    return state == AlarmState::ActiveUnacknowledged
        || state == AlarmState::ActiveAcknowledged;
}

bool QmlAlarmModel::isAcknowledgeable(AlarmState state)
{
    return state == AlarmState::ActiveUnacknowledged
        || state == AlarmState::RecoveredUnacknowledged;
}

QString QmlAlarmModel::severityText(AlarmSeverity severity)
{
    return severity == AlarmSeverity::Critical
        ? QStringLiteral("严重 (Critical)")
        : QStringLiteral("警告 (Warning)");
}

QString QmlAlarmModel::stateText(AlarmState state)
{
    switch (state)
    {
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

QList<AlarmRecord> QmlAlarmModel::visibleRecords() const
{
    if (m_mode == Mode::History)
    {
        return m_records;
    }

    QList<AlarmRecord> active;
    for (const auto &record : m_records)
    {
        if (isActive(record.state))
        {
            active.append(record);
        }
    }
    return active;
}
