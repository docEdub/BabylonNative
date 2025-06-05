if(NOT ANDROID)
    add_subdirectory(Playground)
endif()

if((WIN32 AND NOT WINDOWS_STORE) OR (APPLE AND NOT IOS AND NOT VISIONOS) OR (UNIX AND NOT ANDROID AND NOT APPLE))
    add_subdirectory(UnitTests)
endif()

# Add Clipchamp-specific tests
if(APPLE OR (UNIX AND NOT ANDROID))
    add_subdirectory(ClipchampTests)
endif()

npm(install --silent)
