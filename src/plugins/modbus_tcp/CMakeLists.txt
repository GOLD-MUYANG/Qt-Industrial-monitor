add_library(modbus_tcp_codec STATIC
    ModbusRegisterCodec.cpp
    ModbusRegisterCodec.h
)

target_include_directories(modbus_tcp_codec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(modbus_tcp_codec PUBLIC industrial::protocol_sdk)
set_target_properties(modbus_tcp_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(modbus_tcp_plugin MODULE
    ModbusTcpPlugin.cpp
    ModbusTcpPlugin.h
    ModbusTcpWorker.cpp
    ModbusTcpWorker.h
    modbus_tcp.json
)

target_link_libraries(modbus_tcp_plugin PRIVATE
    modbus_tcp_codec
    Qt6::SerialBus
)
set_target_properties(modbus_tcp_plugin PROPERTIES
    OUTPUT_NAME modbus_tcp_plugin
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)
