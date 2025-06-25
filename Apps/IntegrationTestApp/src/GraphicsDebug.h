#pragma once

// Debug defaults

#ifdef _DEBUG
#ifndef BABYLON_GRAPHICS_DEBUG
#define BABYLON_GRAPHICS_DEBUG 1
#endif
#ifndef BABYLON_PRINT_DEBUG
#define BABYLON_PRINT_DEBUG 1
#endif
#endif

// Graphics debug

#ifdef BABYLON_GRAPHICS_DEBUG

void GraphicsDebug_Load();
void GraphicsDebug_BeginFrameCapture() noexcept;
void GraphicsDebug_EndFrameCapture() noexcept;

#define BABYLON_GRAPHICS_DEBUG_INIT()                GraphicsDebug_Load()
#define BABYLON_GRAPHICS_DEBUG_BEGIN_FRAME_CAPTURE() GraphicsDebug_BeginFrameCapture()
#define BABYLON_GRAPHICS_DEBUG_END_FRAME_CAPTURE()   GraphicsDebug_EndFrameCapture()

#else

#define BABYLON_GRAPHICS_DEBUG_INIT()
#define BABYLON_GRAPHICS_DEBUG_BEGIN_FRAME_CAPTURE()
#define BABYLON_GRAPHICS_DEBUG_END_FRAME_CAPTURE()

#endif

// Print debug

#ifdef BABYLON_PRINT_DEBUG

void PrintSubMessage(const char* format, ...) noexcept;

#define BABYLON_PRINT_CONSOLE(format, ...) PrintSubMessage( format, __VA_ARGS__ )

#else

#define BABYLON_PRINT_CONSOLE(format, ...) ( void )__VA_ARGS__

#endif