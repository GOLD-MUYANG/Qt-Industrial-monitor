#pragma once

#include "RollingStatistics.h"

#include <QHash>
#include <QObject>

#include <industrial/protocol/ProtocolTypes.h>

class DataPipeline final : public QObject
{
    Q_OBJECT

public:
    explicit DataPipeline(QObject *parent = nullptr);

public slots:
    void processSamples(const industrial::protocol::SampleBatch &samples);
    void handleDeviceState(const industrial::protocol::DeviceState &state);

signals:
    void snapshotsReady(
        const industrial::protocol::RealtimeSnapshotBatch &snapshots);
    void pipelineError(const industrial::protocol::DeviceError &error);

private:
    struct TagState
    {
        RollingStatistics statistics;
        industrial::protocol::RealtimeSnapshot lastSnapshot;
        bool hasSnapshot = false;
    };

    struct DeviceData
    {
        QHash<QString, TagState> tags;
        quint64 lastSequence = 0;
        QDateTime lastTimestampUtc;
        industrial::protocol::ConnectionState connectionState =
            industrial::protocol::ConnectionState::Stopped;
    };

    static bool isKnownTag(const QString &tagId);
    static bool isInRange(const industrial::protocol::MeasurementSample &sample);
    void emitDataError(const QString &deviceId,
                       quint64 sequence,
                       const QString &message);

    QHash<QString, DeviceData> m_devices;
};
