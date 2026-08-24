add_library(protocol_sdk SHARED
    ProtocolTypes.cpp
    include/industrial/protocol/AbstractDeviceWorker.h
    include/industrial/protocol/IProtocolPlugin.h
    include/industrial/protocol/ProtocolSdkExport.h
    include/industrial/protocol/ProtocolTypes.h
    include/industrial/protocol/ProtocolVersion.h
)
add_library(industrial::protocol_sdk ALIAS protocol_sdk)

target_compile_definitions(protocol_sdk PRIVATE PROTOCOL_SDK_LIBRARY)
target_include_directories(protocol_sdk
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(protocol_sdk PUBLIC Qt6::Core)

set_target_properties(protocol_sdk PROPERTIES
    OUTPUT_NAME protocol_sdk
    VERSION "${PROJECT_VERSION}"
    SOVERSION 0
)
