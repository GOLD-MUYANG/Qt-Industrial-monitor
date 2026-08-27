add_library(industrial_ui_support STATIC
    ChineseFonts.qrc
    UiFont.cpp
    UiFont.h
)

target_include_directories(industrial_ui_support PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(industrial_ui_support PUBLIC Qt6::Widgets)
