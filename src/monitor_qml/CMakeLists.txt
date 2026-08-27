add_library(monitor_qml_ui STATIC
    bridge/QmlApplicationFacade.cpp
    bridge/QmlApplicationFacade.h
    models/QmlRealtimeModel.cpp
    models/QmlRealtimeModel.h
    models/QmlTrendModel.cpp
    models/QmlTrendModel.h
    models/QmlDeviceViewModel.cpp
    models/QmlDeviceViewModel.h
    models/QmlProtocolModel.cpp
    models/QmlProtocolModel.h
    models/QmlAlarmModel.cpp
    models/QmlAlarmModel.h
)

target_include_directories(monitor_qml_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
    "${CMAKE_CURRENT_SOURCE_DIR}/models"
)
target_link_libraries(monitor_qml_ui PUBLIC
    monitor_runtime
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
)

set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(qml/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS "Main.qml"
)
set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_RESOURCE_ALIAS "Theme.qml"
)
set_source_files_properties(qml/pages/RealtimePage.qml PROPERTIES
    QT_RESOURCE_ALIAS "pages/RealtimePage.qml"
)
set_source_files_properties(qml/pages/DevicePage.qml PROPERTIES
    QT_RESOURCE_ALIAS "pages/DevicePage.qml"
)
set_source_files_properties(qml/pages/AlarmPage.qml PROPERTIES
    QT_RESOURCE_ALIAS "pages/AlarmPage.qml"
)
set_source_files_properties(qml/components/StatusBadge.qml PROPERTIES
    QT_RESOURCE_ALIAS "components/StatusBadge.qml"
)
set_source_files_properties(qml/components/MetricCard.qml PROPERTIES
    QT_RESOURCE_ALIAS "components/MetricCard.qml"
)
set_source_files_properties(qml/components/LabeledField.qml PROPERTIES
    QT_RESOURCE_ALIAS "components/LabeledField.qml"
)
set_source_files_properties(qml/components/SectionPanel.qml PROPERTIES
    QT_RESOURCE_ALIAS "components/SectionPanel.qml"
)

qt_add_qml_module(monitor_qml_ui
    URI IndustrialMonitor.Qml
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/IndustrialMonitor/Qml"
    NO_PLUGIN
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/pages/RealtimePage.qml
        qml/pages/DevicePage.qml
        qml/pages/AlarmPage.qml
        qml/components/StatusBadge.qml
        qml/components/MetricCard.qml
        qml/components/LabeledField.qml
        qml/components/SectionPanel.qml
)

qt_add_executable(industrial_monitor_qml main.cpp)
target_link_libraries(industrial_monitor_qml PRIVATE
    industrial_ui_support
    monitor_qml_ui
    Qt6::Charts
    Qt6::QuickControls2
    Qt6::Widgets
)
