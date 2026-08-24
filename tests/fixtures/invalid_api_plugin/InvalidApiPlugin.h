#pragma once

#include <QObject>

#include <industrial/protocol/IProtocolPlugin.h>

class InvalidApiPlugin final
    : public QObject
    , public industrial::protocol::IProtocolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID INDUSTRIAL_PROTOCOL_PLUGIN_IID
                      FILE "invalid_api_plugin.json")
    Q_INTERFACES(industrial::protocol::IProtocolPlugin)

public:
    industrial::protocol::ProtocolDescriptor descriptor() const override;
    industrial::protocol::AbstractDeviceWorker *createWorker(
        const industrial::protocol::DeviceConfig &config) override;
};
