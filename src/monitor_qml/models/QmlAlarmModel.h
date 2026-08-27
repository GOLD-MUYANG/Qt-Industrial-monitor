#pragma once

#include <QAbstractListModel>

#include "AlarmTypes.h"

class QmlAlarmModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum class Mode
    {
        Active,
        History
    };
    Q_ENUM(Mode)

    enum Role
    {
        AlarmIdRole = Qt::UserRole + 1,
        ActivatedAtTextRole,
        DeviceIdRole,
        TagTextRole,
        MessageRole,
        TriggerValueRole,
        SeverityRole,
        SeverityTextRole,
        StateRole,
        StateTextRole,
        ActiveRole,
        AcknowledgeableRole
    };
    Q_ENUM(Role)

    explicit QmlAlarmModel(Mode mode, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString alarmIdAt(int row) const;
    Q_INVOKABLE bool isAcknowledgeableId(const QString &alarmId) const;

public slots:
    void upsertAlarm(const industrial::monitor::AlarmRecord &alarm);

private:
    static bool isActive(industrial::monitor::AlarmState state);
    static bool isAcknowledgeable(industrial::monitor::AlarmState state);
    static QString severityText(industrial::monitor::AlarmSeverity severity);
    static QString stateText(industrial::monitor::AlarmState state);
    QList<industrial::monitor::AlarmRecord> visibleRecords() const;

    Mode m_mode;
    QList<industrial::monitor::AlarmRecord> m_records;
};
