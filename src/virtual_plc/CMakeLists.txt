add_library(virtual_plc_core STATIC
    RegisterBank.cpp
    RegisterBank.h
    SimulationEngine.cpp
    SimulationEngine.h
    VirtualPlcServer.cpp
    VirtualPlcServer.h
)

target_include_directories(virtual_plc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(virtual_plc_core PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::SerialBus
)

add_library(virtual_plc_ui STATIC
    ChineseFonts.qrc
    UiFont.cpp
    UiFont.h
    VirtualPlcWindow.cpp
    VirtualPlcWindow.h
)
target_include_directories(virtual_plc_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(virtual_plc_ui PUBLIC
    virtual_plc_core
    Qt6::Widgets
)

add_executable(virtual_plc main.cpp)
target_link_libraries(virtual_plc PRIVATE virtual_plc_ui)
