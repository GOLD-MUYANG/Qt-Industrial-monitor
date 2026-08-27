#include "RealtimeTableModel.h"

#include <QBrush>
#include <QColor>

using namespace industrial::protocol;

RealtimeTableModel::RealtimeTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int RealtimeTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : tagOrder().size();
}

int RealtimeTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant RealtimeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()
        || index.column() < 0 || index.column() >= ColumnCount) {
        return {};
    }
    const QString tagId = tagOrder().at(index.row());
    const auto snapshotIt = m_snapshots.constFind(tagId);
    if (role == Qt::ForegroundRole && snapshotIt != m_snapshots.constEnd()
        && index.column() == QualityColumn) {
        return QBrush(qualityColor(snapshotIt->quality));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (index.column() == TagColumn) {
        return tagName(tagId);
    }
    if (snapshotIt == m_snapshots.constEnd()) {
        return QStringLiteral("—");
    }
    const auto &snapshot = snapshotIt.value();
    switch (index.column()) {
    case CurrentColumn:
        return QString::number(snapshot.current, 'f', 2);
    case MinimumColumn:
        return QString::number(snapshot.minimum, 'f', 2);
    case MaximumColumn:
        return QString::number(snapshot.maximum, 'f', 2);
    case AverageColumn:
        return QString::number(snapshot.average, 'f', 2);
    case QualityColumn:
        return qualityText(snapshot.quality);
    case TimestampColumn:
        return snapshot.timestampUtc.toLocalTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    default:
        return {};
    }
}

QVariant RealtimeTableModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= ColumnCount) {
        return {};
    }
    static const QStringList headers = {
        QStringLiteral("测点"),
        QStringLiteral("当前值"),
        QStringLiteral("最小值"),
        QStringLiteral("最大值"),
        QStringLiteral("平均值"),
        QStringLiteral("质量"),
        QStringLiteral("本地时间"),
    };
    return headers.at(section);
}

void RealtimeTableModel::applySnapshots(const RealtimeSnapshotBatch &snapshots)
{
    for (const auto &snapshot : snapshots) {
        const int row = tagOrder().indexOf(snapshot.tagId);
        if (row < 0) {
            continue;
        }
        m_snapshots.insert(snapshot.tagId, snapshot);
        emit dataChanged(index(row, CurrentColumn), index(row, TimestampColumn));
    }
}

QStringList RealtimeTableModel::tagOrder()
{
    return {
        QStringLiteral("temperature"),
        QStringLiteral("pressure"),
        QStringLiteral("speed"),
        QStringLiteral("voltage"),
        QStringLiteral("status"),
    };
}

QString RealtimeTableModel::tagName(const QString &tagId)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("temperature"), QStringLiteral("温度 / ℃")},
        {QStringLiteral("pressure"), QStringLiteral("压力 / MPa")},
        {QStringLiteral("speed"), QStringLiteral("转速 / rpm")},
        {QStringLiteral("voltage"), QStringLiteral("电压 / V")},
        {QStringLiteral("status"), QStringLiteral("运行状态")},
    };
    return names.value(tagId, tagId);
}

QString RealtimeTableModel::qualityText(DataQuality quality)
{
    switch (quality) {
    case DataQuality::Good:
        return QStringLiteral("正常 (Good)");
    case DataQuality::Stale:
        return QStringLiteral("陈旧 (Stale)");
    case DataQuality::Bad:
        return QStringLiteral("无效 (Bad)");
    }
    return QStringLiteral("未知");
}

QColor RealtimeTableModel::qualityColor(DataQuality quality)
{
    switch (quality) {
    case DataQuality::Good:
        return QColor(QStringLiteral("#137333"));
    case DataQuality::Stale:
        return QColor(QStringLiteral("#9A6700"));
    case DataQuality::Bad:
        return QColor(QStringLiteral("#B42318"));
    }
    return QColor(Qt::black);
}
