#pragma once

#define INDUSTRIAL_PROTOCOL_PLUGIN_IID \
    "com.muyang.IndustrialMonitor.ProtocolPlugin/1.0"

namespace industrial::protocol {

inline constexpr int ProtocolApiVersion = 1;
inline constexpr char ProtocolPluginIid[] = INDUSTRIAL_PROTOCOL_PLUGIN_IID;

} // namespace industrial::protocol
