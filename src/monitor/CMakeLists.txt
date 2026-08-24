add_library(monitor_application STATIC
    application/DeviceSession.cpp
    application/DeviceSession.h
    application/PluginManager.cpp
    application/PluginManager.h
    data/DataPipeline.cpp
    data/DataPipeline.h
    data/RollingStatistics.cpp
    data/RollingStatistics.h
)

target_include_directories(monitor_application PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
)
target_link_libraries(monitor_application PUBLIC
    industrial::protocol_sdk
    Qt6::Core
)

add_executable(industrial_monitor main.cpp)
target_link_libraries(industrial_monitor PRIVATE monitor_application)
