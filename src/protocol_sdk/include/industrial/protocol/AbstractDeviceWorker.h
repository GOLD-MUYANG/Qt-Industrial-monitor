#pragma once

#include <QObject>

#include <industrial/protocol/ProtocolTypes.h>

namespace industrial::protocol {

class PROTOCOL_SDK_EXPORT AbstractDeviceWorker : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~AbstractDeviceWorker() override = default;

public slots:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void writeValue(const WriteRequest &request) = 0;

signals:
    void stateChanged(const DeviceState &state);
    void samplesReady(const SampleBatch &samples);
    void writeFinished(const WriteResult &result);
    void communicationError(const DeviceError &error);
    void transactionLogged(const TransactionLog &log);
    void stopped();
};

} // namespace industrial::protocol
