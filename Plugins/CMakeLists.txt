if(BABYLON_NATIVE_PLUGIN_EXTERNALTEXTURE)
    add_subdirectory(ExternalTexture)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVECAMERA AND NOT (APPLE AND CMAKE_SYSTEM_NAME MATCHES "visionOS"))
    add_subdirectory(NativeCamera)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVECAPTURE)
    add_subdirectory(NativeCapture)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVEENGINE)
    add_subdirectory(NativeEngine)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVEINPUT)
    add_subdirectory(NativeInput)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVEOPTIMIZATIONS)
    add_subdirectory(NativeOptimizations)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVETRACING)
    add_subdirectory(NativeTracing)
endif()

if(BABYLON_NATIVE_PLUGIN_TESTUTILS)
    add_subdirectory(TestUtils)
endif()

if(BABYLON_NATIVE_PLUGIN_NATIVEXR AND (ANDROID OR IOS OR (APPLE AND CMAKE_SYSTEM_NAME MATCHES "visionOS")))
    add_subdirectory(NativeXr)
endif()
