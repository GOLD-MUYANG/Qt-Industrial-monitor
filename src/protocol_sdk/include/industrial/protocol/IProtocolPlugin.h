#pragma once

#include <QtPlugin>

#include <industrial/protocol/AbstractDeviceWorker.h>
#include <industrial/protocol/ProtocolVersion.h>

namespace industrial::protocol {

class IProtocolPlugin
{
public:
    virtual ~IProtocolPlugin() = default;

    virtual ProtocolDescriptor descriptor() const = 0;
    virtual AbstractDeviceWorker *createWorker(const DeviceConfig &config) = 0;
};

} // namespace industrial::protocol

Q_DECLARE_INTERFACE(industrial::protocol::IProtocolPlugin,
                    INDUSTRIAL_PROTOCOL_PLUGIN_IID)
