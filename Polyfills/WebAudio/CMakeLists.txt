set(SOURCES
    "Include/Babylon/Polyfills/WebAudio.h"
    "Source/WebAudio.cpp")

add_library(WebAudio ${SOURCES})
# warnings_as_errors(WebAudio)

target_include_directories(WebAudio
    PUBLIC "Include")

target_link_libraries(WebAudio
    PUBLIC napi
    PRIVATE JsRuntimeInternal
    PRIVATE LabSound)

if(APPLE)
    find_library(ACCELERATE Accelerate)
    if(NOT ACCELERATE)
        message(FATAL_ERROR "Accelerate framework not found")
    endif()
    target_link_libraries(WebAudio PRIVATE ${ACCELERATE})
endif()

set_property(TARGET WebAudio PROPERTY FOLDER Polyfills)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})