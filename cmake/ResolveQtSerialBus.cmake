# Ubuntu 22.04 将 SerialBus 的工具和 SerialPort 开发配置拆成独立包，
# 但 Qt6SerialBusConfig.cmake 会把它们视为强制依赖。完整环境优先使用
# Qt 官方包；仅在包配置不完整时验证并导入本项目实际使用的 Modbus 库。
function(industrial_resolve_qt_serial_bus)
    get_filename_component(_qt_package_root "${Qt6_DIR}" DIRECTORY)
    get_filename_component(_qt_import_prefix "${_qt_package_root}" DIRECTORY)
    get_filename_component(_qt_import_prefix "${_qt_import_prefix}" DIRECTORY)
    get_filename_component(_qt_import_prefix "${_qt_import_prefix}" DIRECTORY)

    set(_serial_bus_config
        "${_qt_package_root}/Qt6SerialBus/Qt6SerialBusConfig.cmake")
    set(_serial_port_config
        "${_qt_package_root}/Qt6SerialPort/Qt6SerialPortConfig.cmake")
    set(_serial_bus_tools_targets
        "${_qt_package_root}/Qt6SerialBusTools/Qt6SerialBusToolsTargets-none.cmake")

    set(_incomplete_reasons)
    if(NOT EXISTS "${_serial_bus_config}")
        list(APPEND _incomplete_reasons "Qt6SerialBusConfig.cmake 缺失")
    endif()
    if(NOT EXISTS "${_serial_port_config}")
        list(APPEND _incomplete_reasons "Qt6SerialPort 开发配置缺失")
    endif()

    if(EXISTS "${_serial_bus_tools_targets}")
        file(STRINGS "${_serial_bus_tools_targets}" _tool_location_line
            REGEX "IMPORTED_LOCATION_NONE.*canbusutil")
        if(_tool_location_line)
            string(REGEX REPLACE
                ".*IMPORTED_LOCATION_NONE \"([^\"]+)\".*" "\\1"
                _packaged_tool "${_tool_location_line}")
            string(REPLACE "\${_IMPORT_PREFIX}" "${_qt_import_prefix}"
                _packaged_tool "${_packaged_tool}")
            if(NOT EXISTS "${_packaged_tool}")
                list(APPEND _incomplete_reasons "canbusutil 缺失")
            endif()
        endif()
    endif()

    if(NOT _incomplete_reasons)
        find_package(Qt6SerialBus 6.2 REQUIRED)
        return()
    endif()

    find_path(Qt6SerialBus_INCLUDE_DIR
        NAMES QtSerialBus/QModbusTcpClient
        PATH_SUFFIXES x86_64-linux-gnu/qt6 qt6
    )
    find_library(Qt6SerialBus_LIBRARY NAMES Qt6SerialBus)

    if(NOT Qt6SerialBus_INCLUDE_DIR OR NOT Qt6SerialBus_LIBRARY)
        message(FATAL_ERROR
            "Qt Serial Bus 的开发头文件或共享库缺失；请安装完整的 Qt 6 Serial Bus 开发包。")
    endif()

    file(STRINGS
        "${Qt6SerialBus_INCLUDE_DIR}/QtSerialBus/qtserialbusversion.h"
        _version_line REGEX "QTSERIALBUS_VERSION_STR")
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
        Qt6SerialBus_VERSION "${_version_line}")
    if(NOT Qt6SerialBus_VERSION OR Qt6SerialBus_VERSION VERSION_LESS 6.2)
        message(FATAL_ERROR
            "Qt Serial Bus 版本 ${Qt6SerialBus_VERSION} 低于要求的 6.2。")
    endif()

    list(JOIN _incomplete_reasons "、" _incomplete_reason_text)
    message(WARNING
        "Qt SerialBus CMake 包不完整（${_incomplete_reason_text}）；"
        "已验证 ${Qt6SerialBus_VERSION} 头文件并直接导入 ${Qt6SerialBus_LIBRARY}。"
    )

    add_library(Qt6::SerialBus SHARED IMPORTED)
    set_target_properties(Qt6::SerialBus PROPERTIES
        IMPORTED_LOCATION "${Qt6SerialBus_LIBRARY}"
        INTERFACE_COMPILE_DEFINITIONS QT_SERIALBUS_LIB
        INTERFACE_INCLUDE_DIRECTORIES
            "${Qt6SerialBus_INCLUDE_DIR};${Qt6SerialBus_INCLUDE_DIR}/QtSerialBus"
        INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::Network"
    )
endfunction()
