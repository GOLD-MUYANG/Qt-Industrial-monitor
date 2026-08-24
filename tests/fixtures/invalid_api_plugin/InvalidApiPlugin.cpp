#include "InvalidApiPlugin.h"

industrial::protocol::ProtocolDescriptor InvalidApiPlugin::descriptor() const
{
    return {
        QStringLiteral("invalid-api"),
        QStringLiteral("Invalid API fixture"),
        999,
        {}
    };
}

industrial::protocol::AbstractDeviceWorker *InvalidApiPlugin::createWorker(
    const industrial::protocol::DeviceConfig &config)
{
    Q_UNUSED(config)
    return nullptr;
}
