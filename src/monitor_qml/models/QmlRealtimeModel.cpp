#include "QmlRealtimeModel.h"

using namespace industrial::protocol;

QmlRealtimeModel::QmlRealtimeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int QmlRealtimeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : tagDefinitions().size();
}

QVariant QmlRealtimeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const auto &tag = tagDefinitions().at(index.row());
    if (role == TagIdRole)
    {
        return tag.id;
    }
    if (role == DisplayNameRole)
    {
        return tag.displayName;
    }
    if (role == UnitRole)
    {
        return tag.unit;
    }

    const auto snapshot = m_snapshots.constFind(tag.id);
    if (snapshot == m_snapshots.constEnd())
    {
        if (role == QualityRole)
        {
            return static_cast<int>(DataQuality::Bad);
        }
        if (role == QualityTextRole)
        {
            return QStringLiteral("尚无数据");
        }
        if (role == TimestampTextRole)
        {
            return QStringLiteral("尚无数据");
        }
        return {};
    }

    switch (role)
    {
    case CurrentValueRole:
        return snapshot->current;
    case MinimumValueRole:
        return snapshot->minimum;
    case MaximumValueRole:
        return snapshot->maximum;
    case AverageValueRole:
        return snapshot->average;
    case QualityRole:
        return static_cast<int>(snapshot->quality);
    case QualityTextRole:
        return qualityText(snapshot->quality);
    case TimestampTextRole:
        return snapshot->timestampUtc.toLocalTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlRealtimeModel::roleNames() const
{
    return {
        {TagIdRole, "tagId"},
        {DisplayNameRole, "displayName"},
        {UnitRole, "unit"},
        {CurrentValueRole, "currentValue"},
        {MinimumValueRole, "minimumValue"},
        {MaximumValueRole, "maximumValue"},
        {AverageValueRole, "averageValue"},
        {QualityRole, "quality"},
        {QualityTextRole, "qualityText"},
        {TimestampTextRole, "timestampText"},
    };
}

void QmlRealtimeModel::applySnapshots(const RealtimeSnapshotBatch &snapshots)
{
    for (const auto &snapshot : snapshots)
    {
        const auto &definitions = tagDefinitions();
        int row = -1;
        for (int index = 0; index < definitions.size(); ++index)
        {
            if (definitions.at(index).id == snapshot.tagId)
            {
                row = index;
                break;
            }
        }
        if (row < 0)
        {
            continue;
        }

        m_snapshots.insert(snapshot.tagId, snapshot);
        emit dataChanged(
            index(row), index(row),
            {CurrentValueRole, MinimumValueRole, MaximumValueRole,
             AverageValueRole, QualityRole, QualityTextRole,
             TimestampTextRole});
    }
}

const QList<QmlRealtimeModel::TagDefinition> &QmlRealtimeModel::tagDefinitions()
{
    static const QList<TagDefinition> definitions = {
        {QStringLiteral("temperature"), QStringLiteral("温度"), QStringLiteral("℃")},
        {QStringLiteral("pressure"), QStringLiteral("压力"), QStringLiteral("MPa")},
        {QStringLiteral("speed"), QStringLiteral("转速"), QStringLiteral("rpm")},
        {QStringLiteral("voltage"), QStringLiteral("电压"), QStringLiteral("V")},
        {QStringLiteral("status"), QStringLiteral("运行状态"), QString()},
    };
    return definitions;
}

QString QmlRealtimeModel::qualityText(DataQuality quality)
{
    switch (quality)
    {
    case DataQuality::Good:
        return QStringLiteral("正常 (Good)");
    case DataQuality::Stale:
        return QStringLiteral("陈旧 (Stale)");
    case DataQuality::Bad:
        return QStringLiteral("无效 (Bad)");
    }
    return QStringLiteral("未知");
}
