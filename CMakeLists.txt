cmake_minimum_required(VERSION 3.22)

project(IndustrialMonitoring VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Charts
    Core
    Network
    Qml
    Quick
    QuickControls2
    Sql
    Test
    Widgets
)

include(cmake/ResolveQtSerialBus.cmake)
industrial_resolve_qt_serial_bus()

include(CTest)

add_subdirectory(src/protocol_sdk)
add_subdirectory(src/ui_support)
add_subdirectory(src/virtual_plc)
add_subdirectory(src/plugins/modbus_tcp)
add_subdirectory(src/monitor)
add_subdirectory(src/monitor_qml)

include(cmake/InstallLayout.cmake)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
