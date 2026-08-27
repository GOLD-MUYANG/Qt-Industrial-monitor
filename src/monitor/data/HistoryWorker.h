#pragma once

#include "StorageTypes.h"

#include <QObject>
#include <QSqlDatabase>

class HistoryWorker final : public QObject
{
    Q_OBJECT

public:
    explicit HistoryWorker(QObject *parent = nullptr);

public slots:
    void start(const QString &databasePath);
    void query(const industrial::monitor::HistoryQuery &request);
    void stop();

signals:
    void ready(quintptr threadId);
    void queryFinished(const industrial::monitor::HistoryQueryResult &result);
    void historyError(const QString &message);
    void stopped();

private:
    void closeDatabase();

    QString m_connectionName;
    QSqlDatabase m_database;
};
