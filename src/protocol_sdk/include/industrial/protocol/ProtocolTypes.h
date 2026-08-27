#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <industrial/protocol/ProtocolSdkExport.h>

namespace industrial::protocol {

enum class DataQuality : quint8
{
    Good,
    Stale,
    Bad
};

enum class ConnectionState : quint8
{
    Stopped,
    Connecting,
    Online,
    Reconnecting,
    Stopping,
    Faulted
};

enum class DeviceErrorCategory : quint8
{
    Configuration,
    Connection,
    Timeout,
    Protocol,
    Data,
    Lifecycle
};

struct DeviceConfig
{
    QString id;
    QString name;
    QString protocolKey;
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 1502;
    int unitId = 1;
    int timeoutMs = 800;
    int protocolRetries = 1;
    int pollIntervalMs = 500;
    int consecutiveFailureLimit = 3;
    QList<int> reconnectDelaysMs = {1'000, 2'000, 4'000, 8'000, 10'000};
    bool enabled = true;
};

struct ProtocolDescriptor
{
    QString key;
    QString displayName;
    int apiVersion = 0;
    QStringList capabilities;
};

struct DeviceState
{
    QString deviceId;
    ConnectionState connectionState = ConnectionState::Stopped;
    QString message;
};

struct MeasurementSample
{
    QString deviceId;
    QString tagId;
    quint16 rawValue = 0;
    double engineeringValue = 0.0;
    DataQuality quality = DataQuality::Good;
    QDateTime timestampUtc;
    quint64 sequence = 0;
};

using SampleBatch = QList<MeasurementSample>;

struct WriteRequest
{
    QString deviceId;
    QString tagId;
    quint16 address = 10;
    quint16 rawValue = 0;
    quint64 requestId = 0;
};

struct WriteResult
{
    quint64 requestId = 0;
    bool success = false;
    QString errorMessage;
};

struct DeviceError
{
    QString deviceId;
    int code = 0;
    QString message;
    DeviceErrorCategory category = DeviceErrorCategory::Connection;
    quint64 requestId = 0;
    bool recoverable = true;
};

struct TransactionLog
{
    QString deviceId;
    quint64 requestId = 0;
    int functionCode = 0;
    quint16 startAddress = 0;
    quint16 valueCount = 0;
    qint64 elapsedMs = 0;
    bool success = false;
    QString errorMessage;
    bool skipped = false;
};

struct RealtimeSnapshot
{
    QString deviceId;
    QString tagId;
    double current = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    int sampleCount = 0;
    DataQuality quality = DataQuality::Bad;
    QDateTime timestampUtc;
    quint64 sequence = 0;
};

using RealtimeSnapshotBatch = QList<RealtimeSnapshot>;

PROTOCOL_SDK_EXPORT void registerProtocolMetaTypes();

} // namespace industrial::protocol

Q_DECLARE_METATYPE(industrial::protocol::DeviceConfig)
Q_DECLARE_METATYPE(industrial::protocol::DeviceErrorCategory)
Q_DECLARE_METATYPE(industrial::protocol::ProtocolDescriptor)
Q_DECLARE_METATYPE(industrial::protocol::DeviceState)
Q_DECLARE_METATYPE(industrial::protocol::MeasurementSample)
Q_DECLARE_METATYPE(industrial::protocol::SampleBatch)
Q_DECLARE_METATYPE(industrial::protocol::WriteRequest)
Q_DECLARE_METATYPE(industrial::protocol::WriteResult)
Q_DECLARE_METATYPE(industrial::protocol::DeviceError)
Q_DECLARE_METATYPE(industrial::protocol::TransactionLog)
Q_DECLARE_METATYPE(industrial::protocol::RealtimeSnapshot)
Q_DECLARE_METATYPE(industrial::protocol::RealtimeSnapshotBatch)
