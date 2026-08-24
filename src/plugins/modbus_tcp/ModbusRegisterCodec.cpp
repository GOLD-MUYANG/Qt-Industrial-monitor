#include "ModbusRegisterCodec.h"

#include <array>

using industrial::protocol::DataQuality;
using industrial::protocol::MeasurementSample;

namespace
{

MeasurementSample makeSample(const QString &deviceId,
                             const QString &tagId,
                             quint16 rawValue,
                             double engineeringValue,
                             const QDateTime &timestampUtc,
                             quint64 sequence)
{
    return MeasurementSample{deviceId,          tagId,        rawValue, engineeringValue,
                             DataQuality::Good, timestampUtc, sequence};
}

} // namespace

// 负责把协议原始值转换成项目通用
ModbusRegisterCodec::DecodeResult
ModbusRegisterCodec::decodeSnapshot(const QList<quint16> &registers,
                                    const QString &deviceId,
                                    const QDateTime &timestampUtc,
                                    quint64 sequence)
{
    if (registers.size() != 5)
    {
        return {false,
                {},
                QStringLiteral("Expected 5 holding registers, received %1").arg(registers.size())};
    }

    const qint16 signedTemperature = static_cast<qint16>(registers.at(0));
    DecodeResult result;
    result.success = true;
    result.samples.reserve(5);
    result.samples.append(makeSample(deviceId, QStringLiteral("temperature"), registers.at(0),
                                     signedTemperature / 10.0, timestampUtc, sequence));
    result.samples.append(makeSample(deviceId, QStringLiteral("pressure"), registers.at(1),
                                     registers.at(1) / 100.0, timestampUtc, sequence));
    result.samples.append(makeSample(deviceId, QStringLiteral("speed"), registers.at(2),
                                     registers.at(2), timestampUtc, sequence));
    result.samples.append(makeSample(deviceId, QStringLiteral("voltage"), registers.at(3),
                                     registers.at(3) / 10.0, timestampUtc, sequence));
    result.samples.append(makeSample(deviceId, QStringLiteral("status"), registers.at(4),
                                     registers.at(4), timestampUtc, sequence));
    return result;
}
