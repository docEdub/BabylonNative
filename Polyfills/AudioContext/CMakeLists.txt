set(SOURCES
    "Include/Babylon/Polyfills/AudioContext.h"
    "Source/AudioContext.cpp")

add_library(AudioContext ${SOURCES})
warnings_as_errors(AudioContext)

target_include_directories(AudioContext
    PUBLIC "Include")

target_link_libraries(AudioContext
    PUBLIC napi
    PRIVATE JsRuntimeInternal
    PRIVATE LabSound)

set_property(TARGET AudioContext PROPERTY FOLDER Polyfills)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})