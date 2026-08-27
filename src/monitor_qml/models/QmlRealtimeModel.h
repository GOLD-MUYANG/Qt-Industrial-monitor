#pragma once

#include <QAbstractListModel>
#include <QHash>

#include <industrial/protocol/ProtocolTypes.h>

class QmlRealtimeModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        TagIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        UnitRole,
        CurrentValueRole,
        MinimumValueRole,
        MaximumValueRole,
        AverageValueRole,
        QualityRole,
        QualityTextRole,
        TimestampTextRole
    };
    Q_ENUM(Role)

    explicit QmlRealtimeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void applySnapshots(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);

private:
    struct TagDefinition
    {
        QString id;
        QString displayName;
        QString unit;
    };

    static const QList<TagDefinition> &tagDefinitions();
    static QString qualityText(industrial::protocol::DataQuality quality);

    QHash<QString, industrial::protocol::RealtimeSnapshot> m_snapshots;
};
