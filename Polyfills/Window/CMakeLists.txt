set(SOURCES
    "Include/Babylon/Polyfills/Window.h"
    "Source/Window.h"
    "Source/Window.cpp")

add_library(Window ${SOURCES})
warnings_as_errors(Window)

target_include_directories(Window
    PUBLIC "Include")

target_link_libraries(Window
    PUBLIC napi
    PRIVATE base-n
    PRIVATE Scheduling
    PRIVATE JsRuntimeInternal
    PRIVATE GraphicsDeviceContext)

set_property(TARGET Window PROPERTY FOLDER Polyfills)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})