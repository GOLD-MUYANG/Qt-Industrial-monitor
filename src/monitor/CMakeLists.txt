add_library(monitor_vision STATIC
    vision/ColorObjectDetector.cpp
    vision/ColorObjectDetector.h
    vision/VideoFileWorker.cpp
    vision/VideoFileWorker.h
    vision/VisionFrameConverter.cpp
    vision/VisionFrameConverter.h
    vision/VisionPlaybackPolicy.cpp
    vision/VisionPlaybackPolicy.h
    vision/VisionSession.cpp
    vision/VisionSession.h
    vision/VisionTypes.cpp
    vision/VisionTypes.h
)
target_include_directories(monitor_vision PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/vision"
    "${OpenCV_INCLUDE_DIRS}"
)
target_link_libraries(monitor_vision PUBLIC
    Qt6::Core
    Qt6::Gui
    opencv_core
    opencv_imgproc
    opencv_videoio
)

add_library(monitor_application STATIC
    application/DeviceSession.cpp
    application/DeviceSession.h
    application/PluginManager.cpp
    application/PluginManager.h
    domain/AlarmEngine.cpp
    domain/AlarmEngine.h
    domain/AlarmTypes.h
    data/DataPipeline.cpp
    data/DataPipeline.h
    data/DefaultConfig.qrc
    data/default_config.json
    data/DatabaseMigrator.cpp
    data/DatabaseMigrator.h
    data/HistoryWorker.cpp
    data/HistoryWorker.h
    data/RollingStatistics.cpp
    data/RollingStatistics.h
    data/StorageTypes.h
    data/StorageWorker.cpp
    data/StorageWorker.h
)

target_include_directories(monitor_application PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "${CMAKE_CURRENT_SOURCE_DIR}/domain"
)
target_link_libraries(monitor_application PUBLIC
    industrial::protocol_sdk
    Qt6::Core
    Qt6::Sql
)

add_library(monitor_presentation STATIC
    presentation/MainWindow.cpp
    presentation/MainWindow.h
    presentation/models/AlarmTableModel.cpp
    presentation/models/AlarmTableModel.h
    presentation/models/DeviceTableModel.cpp
    presentation/models/DeviceTableModel.h
    presentation/models/RealtimeTableModel.cpp
    presentation/models/RealtimeTableModel.h
    presentation/pages/AlarmPage.cpp
    presentation/pages/AlarmPage.h
    presentation/pages/DevicePage.cpp
    presentation/pages/DevicePage.h
    presentation/pages/RealtimePage.cpp
    presentation/pages/RealtimePage.h
    presentation/pages/VisionPage.cpp
    presentation/pages/VisionPage.h
)
target_include_directories(monitor_presentation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/domain"
    "${CMAKE_CURRENT_SOURCE_DIR}/presentation"
    "${CMAKE_CURRENT_SOURCE_DIR}/presentation/models"
    "${CMAKE_CURRENT_SOURCE_DIR}/presentation/pages"
)
target_link_libraries(monitor_presentation PUBLIC
    industrial_ui_support
    monitor_application
    monitor_vision
    Qt6::Charts
    Qt6::Widgets
)

add_library(monitor_runtime STATIC
    application/ApplicationController.cpp
    application/ApplicationController.h
)
target_include_directories(monitor_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "${CMAKE_CURRENT_SOURCE_DIR}/domain"
)
target_link_libraries(monitor_runtime PUBLIC
    monitor_application
    monitor_vision
)

add_executable(industrial_monitor main.cpp)
target_link_libraries(industrial_monitor PRIVATE
    monitor_application
    monitor_presentation
    monitor_runtime
)
