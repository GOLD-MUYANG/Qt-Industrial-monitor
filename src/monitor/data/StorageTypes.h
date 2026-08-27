#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

#include <industrial/protocol/ProtocolTypes.h>

namespace industrial::monitor {

struct HistoryQuery
{
    quint64 requestId = 0;
    QString deviceId;
    QString tagId;
    QDateTime fromUtc;
    QDateTime toUtc;
    int limit = 1'000;
    int offset = 0;
};

struct HistoryPoint
{
    QString deviceId;
    QString tagId;
    double value = 0.0;
    industrial::protocol::DataQuality quality =
        industrial::protocol::DataQuality::Bad;
    QDateTime timestampUtc;
    quint64 sequence = 0;
};

using HistoryPointList = QList<HistoryPoint>;

struct HistoryQueryResult
{
    quint64 requestId = 0;
    bool success = false;
    QString errorMessage;
    HistoryPointList points;
    bool hasMore = false;
};

inline void registerStorageMetaTypes()
{
    qRegisterMetaType<HistoryQuery>();
    qRegisterMetaType<HistoryPoint>();
    qRegisterMetaType<HistoryPointList>();
    qRegisterMetaType<HistoryQueryResult>();
}

} // namespace industrial::monitor

Q_DECLARE_METATYPE(industrial::monitor::HistoryQuery)
Q_DECLARE_METATYPE(industrial::monitor::HistoryPoint)
Q_DECLARE_METATYPE(industrial::monitor::HistoryPointList)
Q_DECLARE_METATYPE(industrial::monitor::HistoryQueryResult)
