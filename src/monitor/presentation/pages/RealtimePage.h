#pragma once

#include <QHash>
#include <QPointF>
#include <QWidget>

#include <industrial/protocol/ProtocolTypes.h>

class QComboBox;
class QDateTimeAxis;
class QLabel;
class QLineSeries;
class QPushButton;
class QSpinBox;
class QTableView;
class QValueAxis;
class RealtimeTableModel;

class RealtimePage final : public QWidget
{
    Q_OBJECT

public:
    explicit RealtimePage(QWidget *parent = nullptr);

    int seriesPointCount(const QString &tagId) const;
    bool isPaused() const;

public slots:
    void applySnapshots(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);
    void setDeviceState(const industrial::protocol::DeviceState &state);
    void setPaused(bool paused);
    void showWriteResult(
        const industrial::protocol::WriteResult &result);

signals:
    void connectRequested();
    void disconnectRequested();
    void writeTargetSpeedRequested(quint16 targetSpeed);
    void pauseChanged(bool paused);

private:
    static bool isNumericTag(const QString &tagId);
    static QString stateText(industrial::protocol::ConnectionState state);
    void appendHistory(const industrial::protocol::RealtimeSnapshot &snapshot);
    void refreshSeries();

    RealtimeTableModel *m_model = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_writeResultLabel = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QSpinBox *m_targetSpeedSpin = nullptr;
    QComboBox *m_tagSelector = nullptr;
    QLineSeries *m_series = nullptr;
    QDateTimeAxis *m_timeAxis = nullptr;
    QValueAxis *m_valueAxis = nullptr;
    QHash<QString, QVector<QPointF>> m_history;
    industrial::protocol::RealtimeSnapshotBatch m_latestSnapshots;
    bool m_paused = false;
};
