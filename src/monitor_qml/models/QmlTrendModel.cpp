#include "QmlTrendModel.h"

using namespace industrial::protocol;

QmlTrendModel::QmlTrendModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int QmlTrendModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visiblePoints.size();
}

int QmlTrendModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

QVariant QmlTrendModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole || !index.isValid() || index.row() < 0
        || index.row() >= m_visiblePoints.size() || index.column() < 0
        || index.column() >= columnCount())
    {
        return {};
    }

    const auto &point = m_visiblePoints.at(index.row());
    return index.column() == 0
        ? QVariant::fromValue(static_cast<double>(point.timestampMs))
        : QVariant::fromValue(point.value);
}

QString QmlTrendModel::selectedTagId() const
{
    return m_selectedTagId;
}

bool QmlTrendModel::displayPaused() const
{
    return m_displayPaused;
}

void QmlTrendModel::applySnapshots(const RealtimeSnapshotBatch &snapshots)
{
    bool selectedTagChanged = false;
    for (const auto &snapshot : snapshots)
    {
        if (!isTrendTag(snapshot.tagId)
            || snapshot.quality != DataQuality::Good
            || !snapshot.timestampUtc.isValid())
        {
            continue;
        }

        auto &points = m_pointsByTag[snapshot.tagId];
        const qint64 timestampMs = snapshot.timestampUtc.toMSecsSinceEpoch();
        points.append({timestampMs, snapshot.current});
        const qint64 cutoffMs = timestampMs - 60'000;
        while (!points.isEmpty()
               && (points.constFirst().timestampMs < cutoffMs
                   || points.size() > 120))
        {
            points.removeFirst();
        }
        selectedTagChanged = selectedTagChanged
            || snapshot.tagId == m_selectedTagId;
    }

    if (selectedTagChanged && !m_displayPaused)
    {
        notifyVisiblePointsChanged();
    }
}

void QmlTrendModel::selectTag(const QString &tagId)
{
    if (!isTrendTag(tagId) || m_selectedTagId == tagId)
    {
        return;
    }

    beginResetModel();
    m_selectedTagId = tagId;
    m_visiblePoints = m_pointsByTag.value(m_selectedTagId);
    endResetModel();
    emit selectedTagIdChanged();
    emit pointCountChanged();
}

void QmlTrendModel::setDisplayPaused(bool paused)
{
    if (m_displayPaused == paused)
    {
        return;
    }

    m_displayPaused = paused;
    emit displayPausedChanged();
    if (!m_displayPaused)
    {
        notifyVisiblePointsChanged();
    }
}

bool QmlTrendModel::isTrendTag(const QString &tagId)
{
    return tagId == QStringLiteral("temperature")
        || tagId == QStringLiteral("pressure")
        || tagId == QStringLiteral("speed")
        || tagId == QStringLiteral("voltage");
}

void QmlTrendModel::notifyVisiblePointsChanged()
{
    beginResetModel();
    m_visiblePoints = m_pointsByTag.value(m_selectedTagId);
    endResetModel();
    emit pointCountChanged();
}
