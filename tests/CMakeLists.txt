add_executable(tst_protocol_types unit/tst_protocol_types.cpp)
target_link_libraries(tst_protocol_types PRIVATE
    industrial::protocol_sdk
    Qt6::Test
)
add_test(NAME unit.protocol_types COMMAND tst_protocol_types)

add_executable(tst_rolling_statistics unit/tst_rolling_statistics.cpp)
target_include_directories(tst_rolling_statistics PRIVATE
    "${PROJECT_SOURCE_DIR}/src/monitor/data"
)
target_link_libraries(tst_rolling_statistics PRIVATE monitor_application Qt6::Test)
add_test(NAME unit.rolling_statistics COMMAND tst_rolling_statistics)

add_executable(tst_data_pipeline unit/tst_data_pipeline.cpp)
target_include_directories(tst_data_pipeline PRIVATE
    "${PROJECT_SOURCE_DIR}/src/monitor/data"
)
target_link_libraries(tst_data_pipeline PRIVATE monitor_application Qt6::Test)
add_test(NAME unit.data_pipeline COMMAND tst_data_pipeline)

add_executable(tst_device_session unit/tst_device_session.cpp)
target_link_libraries(tst_device_session PRIVATE monitor_application Qt6::Test)
add_test(NAME unit.device_session COMMAND tst_device_session)

add_executable(tst_register_bank unit/tst_register_bank.cpp)
target_link_libraries(tst_register_bank PRIVATE virtual_plc_core Qt6::Test)
add_test(NAME unit.register_bank COMMAND tst_register_bank)

add_executable(tst_simulation_engine unit/tst_simulation_engine.cpp)
target_link_libraries(tst_simulation_engine PRIVATE virtual_plc_core Qt6::Test)
add_test(NAME unit.simulation_engine COMMAND tst_simulation_engine)

add_executable(tst_modbus_register_codec unit/tst_modbus_register_codec.cpp)
target_link_libraries(tst_modbus_register_codec PRIVATE
    modbus_tcp_codec
    Qt6::Test
)
add_test(NAME unit.modbus_register_codec COMMAND tst_modbus_register_codec)

# 构建真实错误插件夹具，验证元数据拒绝路径而不是伪造加载结果。
add_library(invalid_api_plugin MODULE
    fixtures/invalid_api_plugin/InvalidApiPlugin.cpp
    fixtures/invalid_api_plugin/InvalidApiPlugin.h
)
target_link_libraries(invalid_api_plugin PRIVATE industrial::protocol_sdk)
set_target_properties(invalid_api_plugin PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_plugins/invalid_api"
)

add_executable(tst_plugin_manager unit/tst_plugin_manager.cpp)
target_compile_definitions(tst_plugin_manager PRIVATE
    INVALID_API_PLUGIN_DIR="$<TARGET_FILE_DIR:invalid_api_plugin>"
)
target_link_libraries(tst_plugin_manager PRIVATE
    monitor_application
    Qt6::Test
)
add_dependencies(tst_plugin_manager invalid_api_plugin)
add_test(NAME unit.plugin_manager COMMAND tst_plugin_manager)

add_executable(tst_virtual_plc_server integration/tst_virtual_plc_server.cpp)
target_link_libraries(tst_virtual_plc_server PRIVATE
    virtual_plc_core
    Qt6::Network
    Qt6::SerialBus
    Qt6::Test
)
add_test(NAME integration.virtual_plc_server COMMAND tst_virtual_plc_server)

add_executable(tst_modbus_plugin_integration
    integration/tst_modbus_plugin_integration.cpp
)
target_compile_definitions(tst_modbus_plugin_integration PRIVATE
    MODBUS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/bin/plugins"
)
target_link_libraries(tst_modbus_plugin_integration PRIVATE
    monitor_application
    virtual_plc_core
    Qt6::Test
)
add_dependencies(tst_modbus_plugin_integration modbus_tcp_plugin)
add_test(NAME integration.modbus_plugin COMMAND tst_modbus_plugin_integration)

add_executable(tst_modbus_tcp_worker integration/tst_modbus_tcp_worker.cpp)
target_compile_definitions(tst_modbus_tcp_worker PRIVATE
    MODBUS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/bin/plugins"
)
target_link_libraries(tst_modbus_tcp_worker PRIVATE
    monitor_application
    virtual_plc_core
    Qt6::Network
    Qt6::Test
)
add_dependencies(tst_modbus_tcp_worker modbus_tcp_plugin)
add_test(NAME integration.modbus_tcp_worker COMMAND tst_modbus_tcp_worker)

add_executable(tst_virtual_plc_ui_font
    unit/tst_virtual_plc_ui_font.cpp
)
target_link_libraries(tst_virtual_plc_ui_font PRIVATE
    virtual_plc_ui
    Qt6::Test
)
add_test(NAME unit.virtual_plc_ui_font COMMAND tst_virtual_plc_ui_font)
set_tests_properties(unit.virtual_plc_ui_font PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
