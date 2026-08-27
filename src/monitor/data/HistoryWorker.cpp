#include "HistoryWorker.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

using namespace industrial::monitor;
using namespace industrial::protocol;

HistoryWorker::HistoryWorker(QObject *parent)
    : QObject(parent)
{
}

void HistoryWorker::start(const QString &databasePath)
{
    if (m_database.isOpen()) {
        emit historyError(QStringLiteral("历史查询连接已经启动"));
        return;
    }
    registerStorageMetaTypes();
    m_connectionName =
        QStringLiteral("history-%1").arg(QUuid::createUuid().toString());
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                           m_connectionName);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    m_database.setDatabaseName(databasePath);
    if (!m_database.open()) {
        const QString error = m_database.lastError().text();
        closeDatabase();
        emit historyError(QStringLiteral("打开 SQLite 只读连接失败：%1").arg(error));
        return;
    }
    QSqlQuery pragma(m_database);
    if (!pragma.exec(QStringLiteral("PRAGMA query_only=ON"))
        || !pragma.exec(QStringLiteral("PRAGMA busy_timeout=500"))) {
        const QString error = pragma.lastError().text();
        closeDatabase();
        emit historyError(QStringLiteral("配置历史查询连接失败：%1").arg(error));
        return;
    }
    emit ready(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

void HistoryWorker::query(const HistoryQuery &request)
{
    HistoryQueryResult result;
    result.requestId = request.requestId;
    const qint64 rangeMs = request.fromUtc.msecsTo(request.toUtc);
    if (!m_database.isOpen()) {
        result.errorMessage = QStringLiteral("历史查询连接未启动");
    } else if (request.deviceId.isEmpty() || request.tagId.isEmpty()
               || !request.fromUtc.isValid() || !request.toUtc.isValid()
               || request.fromUtc.timeSpec() != Qt::UTC
               || request.toUtc.timeSpec() != Qt::UTC
               || rangeMs <= 0 || rangeMs > 24LL * 60 * 60 * 1'000) {
        result.errorMessage =
            QStringLiteral("历史查询必须使用有效 UTC 时间且范围不超过 24 小时");
    } else if (request.limit < 1 || request.limit > 5'000
               || request.offset < 0) {
        result.errorMessage = QStringLiteral("分页 limit/offset 无效");
    } else {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT device_id,tag_id,value,quality,timestamp_utc,sequence "
            "FROM measurement WHERE device_id=:device AND tag_id=:tag "
            "AND timestamp_utc>=:from_time AND timestamp_utc<=:to_time "
            "ORDER BY timestamp_utc,id LIMIT :limit OFFSET :offset"));
        query.bindValue(QStringLiteral(":device"), request.deviceId);
        query.bindValue(QStringLiteral(":tag"), request.tagId);
        query.bindValue(QStringLiteral(":from_time"),
                        request.fromUtc.toMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":to_time"),
                        request.toUtc.toMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":limit"), request.limit + 1);
        query.bindValue(QStringLiteral(":offset"), request.offset);
        if (!query.exec()) {
            result.errorMessage =
                QStringLiteral("历史查询失败：%1").arg(query.lastError().text());
        } else {
            while (query.next()) {
                HistoryPoint point;
                point.deviceId = query.value(0).toString();
                point.tagId = query.value(1).toString();
                point.value = query.value(2).toDouble();
                point.quality =
                    static_cast<DataQuality>(query.value(3).toInt());
                point.timestampUtc = QDateTime::fromMSecsSinceEpoch(
                    query.value(4).toLongLong(), Qt::UTC);
                point.sequence = query.value(5).toULongLong();
                result.points.append(point);
            }
            if (result.points.size() > request.limit) {
                result.points.removeLast();
                result.hasMore = true;
            }
            result.success = true;
        }
    }
    emit queryFinished(result);
}

void HistoryWorker::stop()
{
    closeDatabase();
    emit stopped();
}

void HistoryWorker::closeDatabase()
{
    if (!m_database.isValid()) {
        return;
    }
    const QString connectionName = m_connectionName;
    m_database.close();
    m_database = QSqlDatabase();
    m_connectionName.clear();
    QSqlDatabase::removeDatabase(connectionName);
}
