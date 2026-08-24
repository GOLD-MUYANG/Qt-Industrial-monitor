#pragma once

#include <QList>

#include <industrial/protocol/ProtocolTypes.h>

class ModbusRegisterCodec
{
public:
    struct DecodeResult
    {
        bool success = false;
        industrial::protocol::SampleBatch samples;
        QString errorMessage;
    };

    static DecodeResult decodeSnapshot(const QList<quint16> &registers,
                                       const QString &deviceId,
                                       const QDateTime &timestampUtc,
                                       quint64 sequence);
};
