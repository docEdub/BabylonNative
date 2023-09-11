set(SOURCES
    "Include/Babylon/Polyfills/WebAudio.h"
    "Source/WebAudio.cpp")

add_library(WebAudio ${SOURCES})
# TODO: Cleanup LabSound warnings and re-enable warnings as errors.
# warnings_as_errors(WebAudio)

target_include_directories(WebAudio
    PUBLIC "Include")

target_link_libraries(WebAudio
    PUBLIC napi
    PRIVATE JsRuntimeInternal
    PRIVATE LabSound
    PRIVATE LabSoundRtAudio)

if(APPLE)
    find_library(AUDIO_TOOLBOX AudioToolbox)
    if(NOT AUDIO_TOOLBOX)
        message(FATAL_ERROR "AudioToolbox framework not found")
    endif()
    target_link_libraries(WebAudio PRIVATE ${AUDIO_TOOLBOX})

    find_library(AUDIO_UNIT AudioUnit)
    if(NOT AUDIO_UNIT)
        message(FATAL_ERROR "AudioUnit framework not found")
    endif()
    target_link_libraries(WebAudio PRIVATE ${AUDIO_UNIT})

    find_library(ACCELERATE Accelerate)
    if(NOT ACCELERATE)
        message(FATAL_ERROR "Accelerate framework not found")
    endif()
    target_link_libraries(WebAudio PRIVATE ${ACCELERATE})

    find_library(CORE_AUDIO CoreAudio)
    if(NOT CORE_AUDIO)
        message(FATAL_ERROR "CoreAudio framework not found")
    endif()
    target_link_libraries(WebAudio PRIVATE ${CORE_AUDIO})

    find_library(COCOA Cocoa)
    if(NOT COCOA)
        message(FATAL_ERROR "Cocoa framework not found")
    endif()
    target_link_libraries(WebAudio PRIVATE ${COCOA})
endif()

set_property(TARGET WebAudio PROPERTY FOLDER Polyfills)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})