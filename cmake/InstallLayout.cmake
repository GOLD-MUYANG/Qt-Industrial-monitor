# 只安装可运行闭环所需目标；测试夹具和静态内部库不进入安装目录。
install(TARGETS
    industrial_monitor
    industrial_monitor_qml
    virtual_plc
    protocol_sdk
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

install(TARGETS modbus_tcp_plugin
    LIBRARY DESTINATION bin/plugins
)

install(FILES
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${PROJECT_SOURCE_DIR}/src/virtual_plc/assets/fonts/WENQUANYI-MICRO-HEI-COPYRIGHT"
    "${PROJECT_SOURCE_DIR}/src/virtual_plc/assets/fonts/APACHE-2.0.txt"
    DESTINATION share/doc/industrial_monitoring
)

set_target_properties(industrial_monitor PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
set_target_properties(industrial_monitor_qml PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
set_target_properties(modbus_tcp_plugin PROPERTIES
    INSTALL_RPATH "$ORIGIN/.."
)
