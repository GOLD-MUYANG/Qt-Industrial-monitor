#pragma once

#include <QAbstractTableModel>
#include <QHash>

#include <industrial/protocol/ProtocolTypes.h>

class QmlTrendModel final : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(QString selectedTagId READ selectedTagId NOTIFY selectedTagIdChanged)
    Q_PROPERTY(bool displayPaused READ displayPaused WRITE setDisplayPaused
                   NOTIFY displayPausedChanged)
    Q_PROPERTY(int pointCount READ rowCount NOTIFY pointCountChanged)

public:
    explicit QmlTrendModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    QString selectedTagId() const;
    bool displayPaused() const;

public slots:
    void applySnapshots(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);
    void selectTag(const QString &tagId);
    void setDisplayPaused(bool paused);

signals:
    void selectedTagIdChanged();
    void displayPausedChanged();
    void pointCountChanged();

private:
    struct Point
    {
        qint64 timestampMs = 0;
        double value = 0.0;
    };

    static bool isTrendTag(const QString &tagId);
    void notifyVisiblePointsChanged();

    QHash<QString, QList<Point>> m_pointsByTag;
    QList<Point> m_visiblePoints;
    QString m_selectedTagId = QStringLiteral("temperature");
    bool m_displayPaused = false;
};
