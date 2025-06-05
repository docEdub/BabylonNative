# Build only on supported platforms (matching UnitTests pattern)
if(NOT((WIN32 AND NOT WINDOWS_STORE) OR (APPLE AND NOT IOS) OR (UNIX AND NOT ANDROID)))
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Test source files
set(SOURCES
    ClipchampInitializationTests.cpp
    ClipchampRenderingTests.cpp
    ClipchampCleanupTests.cpp
    ClipchampErrorHandlingTests.cpp
    ClipchampSuperfillIntegrationTests.cpp
    TestMain.cpp
)

# Platform-specific configurations
if(APPLE)
    find_library(JAVASCRIPTCORE_LIBRARY JavaScriptCore)
    find_library(METAL_LIBRARY Metal)
    find_library(METALKIT_LIBRARY MetalKit)
    find_library(FOUNDATION_LIBRARY Foundation)
    find_library(AVFOUNDATION_LIBRARY AVFoundation)
    
    set(ADDITIONAL_LIBRARIES 
        PRIVATE ${JAVASCRIPTCORE_LIBRARY}
        PRIVATE ${METAL_LIBRARY}
        PRIVATE ${METALKIT_LIBRARY}
        PRIVATE ${FOUNDATION_LIBRARY}
        PRIVATE ${AVFOUNDATION_LIBRARY}
    )
endif()

# Add test executable
add_executable(ClipchampBabylonNativeTests ${SOURCES})
set_property(TARGET ClipchampBabylonNativeTests PROPERTY UNITY_BUILD false)

# Link against BabylonNative libraries (matching UnitTests pattern)
target_link_libraries(ClipchampBabylonNativeTests
    PRIVATE AppRuntime
    PRIVATE Console
    PRIVATE ExternalTexture
    PRIVATE GraphicsDevice
    PRIVATE NativeEngine
    PRIVATE ScriptLoader
    PRIVATE Window
    PRIVATE XMLHttpRequest
    PRIVATE gtest_main
    ${ADDITIONAL_LIBRARIES}
)

# Platform-specific configurations
if(APPLE)
    set_target_properties(ClipchampBabylonNativeTests PROPERTIES
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    )
endif()

# Enable testing
enable_testing()
add_test(NAME ClipchampBabylonNativeTests COMMAND ClipchampBabylonNativeTests)

# Copy runtime DLLs if needed (Windows)
add_custom_command(TARGET ClipchampBabylonNativeTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:ClipchampBabylonNativeTests>>,copy,true> $<TARGET_RUNTIME_DLLS:ClipchampBabylonNativeTests> $<TARGET_FILE_DIR:ClipchampBabylonNativeTests> COMMAND_EXPAND_LISTS)
