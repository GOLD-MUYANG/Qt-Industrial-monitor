#pragma once

#include <QAbstractTableModel>
#include <QHash>

#include <industrial/protocol/ProtocolTypes.h>

class RealtimeTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TagColumn,
        CurrentColumn,
        MinimumColumn,
        MaximumColumn,
        AverageColumn,
        QualityColumn,
        TimestampColumn,
        ColumnCount
    };

    explicit RealtimeTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

public slots:
    void applySnapshots(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);

private:
    static QStringList tagOrder();
    static QString tagName(const QString &tagId);
    static QString qualityText(industrial::protocol::DataQuality quality);
    static QColor qualityColor(industrial::protocol::DataQuality quality);

    QHash<QString, industrial::protocol::RealtimeSnapshot> m_snapshots;
};
