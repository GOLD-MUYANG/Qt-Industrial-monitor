#pragma once

#include <QWidget>

#include "AlarmTypes.h"

class AlarmTableModel;
class QLineEdit;
class QTableView;

class AlarmPage final : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmPage(QWidget *parent = nullptr);

public slots:
    void upsertAlarm(const industrial::monitor::AlarmRecord &alarm);

signals:
    void acknowledgeRequested(const QString &alarmId, const QString &note);

private:
    AlarmTableModel *m_activeModel = nullptr;
    AlarmTableModel *m_historyModel = nullptr;
    QTableView *m_activeTable = nullptr;
    QLineEdit *m_noteEdit = nullptr;
};
