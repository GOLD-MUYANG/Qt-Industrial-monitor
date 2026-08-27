#pragma once

#include "AlarmTypes.h"

#include <QAbstractTableModel>

class AlarmTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Mode
    {
        Active,
        History
    };

    enum Column
    {
        TimeColumn,
        DeviceColumn,
        TagColumn,
        MessageColumn,
        TriggerColumn,
        SeverityColumn,
        StateColumn,
        ColumnCount
    };

    explicit AlarmTableModel(Mode mode, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QString alarmIdAt(int row) const;
    industrial::monitor::AlarmRecord alarmAt(int row) const;

public slots:
    void upsertAlarm(const industrial::monitor::AlarmRecord &alarm);

private:
    static bool isActive(industrial::monitor::AlarmState state);
    static QString severityText(industrial::monitor::AlarmSeverity severity);
    static QString stateText(industrial::monitor::AlarmState state);
    static QColor severityColor(industrial::monitor::AlarmSeverity severity);
    QList<industrial::monitor::AlarmRecord> visibleRecords() const;

    Mode m_mode;
    QList<industrial::monitor::AlarmRecord> m_records;
};
