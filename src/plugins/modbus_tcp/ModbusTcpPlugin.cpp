#include "ModbusTcpPlugin.h"

#include "ModbusTcpWorker.h"

industrial::protocol::ProtocolDescriptor ModbusTcpPlugin::descriptor() const
{
    return {
        QStringLiteral("modbus-tcp"),
        QStringLiteral("Modbus TCP"),
        industrial::protocol::ProtocolApiVersion,
        {
            QStringLiteral("read-registers"),
            QStringLiteral("write-register")
        }
    };
}

industrial::protocol::AbstractDeviceWorker *ModbusTcpPlugin::createWorker(
    const industrial::protocol::DeviceConfig &config)
{
    return new ModbusTcpWorker(config);
}
