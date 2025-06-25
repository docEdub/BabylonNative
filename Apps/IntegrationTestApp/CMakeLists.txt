FILE(GLOB ASSETS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "assets/*")

set(INTEGRATION_APP_SOURCES "src/main.cpp" 
    "src/BabylonRenderer.cpp"
    "src/BabylonRenderer.h"
    "src/BabylonMath.h"
    "src/stb_image_write.h"
    "src/GraphicsDebug.h"
    "src/GraphicsDebug.cpp")

add_executable(IntegrationTestApp ${INTEGRATION_APP_SOURCES} ${ASSETS})
target_include_directories(IntegrationTestApp PRIVATE src)

target_link_libraries(IntegrationTestApp
    PRIVATE AppRuntime
    PRIVATE Console
    PRIVATE ExternalTexture
    PRIVATE GraphicsDevice
    PRIVATE NativeEngine
    PRIVATE ScriptLoader
    PRIVATE Window
    PRIVATE XMLHttpRequest)

foreach(asset ${ASSETS})
    get_filename_component(ASSET_NAME "${asset}" NAME)
    add_custom_command(
        OUTPUT "${CMAKE_CFG_INTDIR}/assets/${ASSET_NAME}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${asset}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/assets/${ASSET_NAME}"
        COMMENT "Copying ${ASSET_NAME}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${asset}")
endforeach()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ASSETS})

target_compile_definitions(IntegrationTestApp PRIVATE NOMINMAX)