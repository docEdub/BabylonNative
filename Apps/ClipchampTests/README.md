# Clipchamp BabylonNative Tests

This test suite provides comprehensive testing of Clipchamp's specific usage patterns of BabylonNative, covering the initialization, rendering, and shutdown patterns used in the Clipchamp mobile app.

## Overview

Clipchamp uses BabylonNative as the rendering engine for their Superfill video compositor. This test suite validates the integration patterns that Clipchamp uses, ensuring that:

1. **Initialization sequences** work correctly with Graphics::Device, AppRuntime, Polyfills, and Plugins
2. **Rendering lifecycle** operates properly with startRenderingNextFrame/finishRenderingCurrentFrame patterns
3. **Superfill integration** functions correctly with project loading, playback, and delegate interactions
4. **Error handling** and cleanup procedures work as expected
5. **Shutdown sequences** complete without memory leaks or crashes

## Test Categories

### 1. Initialization Tests (`ClipchampInitializationTests.cpp`)

Tests the complete BabylonNative initialization sequence that matches Clipchamp's `BabylonNativeBridge.mm`:

- **Graphics Device Setup**: Metal device creation, MTKView configuration
- **Device Update Initialization**: Render loop preparation
- **App Runtime Creation**: JavaScript environment setup
- **Babylon Services**: Polyfills (Console, Window, XMLHttpRequest) and Plugins (NativeEngine)
- **Validation**: Parameter validation, error handling, initialization order dependencies

Key test cases:
- `CompleteInitializationSequence`: Full initialization flow
- `InitializationWithDifferentViewportSizes`: Mobile portrait/landscape dimensions
- `InvalidDimensionsHandling`: Error validation (zero/negative dimensions)
- `InitializationOrderDependency`: Component dependency validation
- `PreventDoubleInitialization`: Multiple initialization protection

### 2. Rendering Tests (`ClipchampRenderingTests.cpp`)

Tests the frame rendering lifecycle that Clipchamp uses for real-time preview and export:

- **Frame Lifecycle**: StartRenderingNextFrame/FinishRenderingCurrentFrame patterns
- **Render Loop**: Multi-frame rendering sequences
- **FPS Calculation**: Performance monitoring (matches `calculateMeanEditorPreviewFps`)
- **Frame State Management**: Prevention of double-start/finish scenarios
- **Concurrency Safety**: Thread-safe frame operations

Key test cases:
- `BasicFrameRenderingLifecycle`: Single frame render cycle
- `PreventDoubleFrameStart`: Concurrent frame protection
- `MultipleFrameSequence`: Sequential frame rendering
- `RenderLoopSimulation`: Realistic rendering scenarios
- `FpsCalculation`: Performance measurement accuracy

### 3. Cleanup Tests (`ClipchampCleanupTests.cpp`)

Tests the cleanup and shutdown sequences that prevent memory leaks and ensure proper resource deallocation:

- **Resource Cleanup**: External textures, device objects, runtime cleanup
- **Order Dependencies**: Proper shutdown sequence (reverse of initialization)
- **Pending Frame Handling**: Cleanup with active rendering
- **Idempotent Cleanup**: Multiple cleanup call safety
- **Memory Leak Prevention**: Resource lifecycle validation

Key test cases:
- `BasicCleanupSequence`: Standard shutdown flow
- `CleanupWithPendingFrame`: Cleanup during active rendering
- `CleanupWithExternalTextures`: Texture resource management
- `MultipleCleanupCalls`: Idempotent cleanup behavior
- `MemoryLeakPrevention`: Resource leak detection

### 4. Error Handling Tests (`ClipchampErrorHandlingTests.cpp`)

Tests error scenarios and recovery patterns that match Clipchamp's error handling in `BabylonNativeBridge.mm`:

- **Parameter Validation**: Dimension validation, null pointer checks
- **Initialization Errors**: Device creation failures, double initialization
- **Runtime Errors**: JavaScript errors, script loading failures
- **Error Message Formatting**: Consistent error reporting
- **Recovery Scenarios**: Error recovery and retry patterns

Key test cases:
- `InvalidDimensionHandling`: Parameter validation errors
- `DoubleInitializationPrevention`: Multiple initialization errors
- `ScriptLoadingErrorScenarios`: JavaScript loading failures
- `ErrorMessageFormatting`: Consistent error format validation
- `ErrorRecoveryScenarios`: Recovery from error states

### 5. Superfill Integration Tests (`ClipchampSuperfillIntegrationTests.cpp`)

Tests the Superfill compositor integration patterns that are core to Clipchamp's video editing functionality:

- **Project Lifecycle**: Project loading, source creation, playback states
- **Source Management**: Video/audio source creation, configuration, destruction
- **Playback Control**: Play/pause/seek operations, progress tracking
- **Audio Management**: Audio stream activation/deactivation
- **Export Workflow**: Frame-by-frame export process
- **Delegate Callbacks**: BabylonNativeBridgeDelegate interaction patterns

Key test cases:
- `BasicProjectLifecycle`: Project load and source creation
- `SourceManagementLifecycle`: Source CRUD operations
- `PlaybackStateManagement`: Playback state transitions
- `FrameReadingAndPlayback`: Frame progression and reading
- `AudioActivationDeactivation`: Audio stream management
- `ExportWorkflow`: Video export process
- `ComplexProjectIntegration`: Multi-track project handling

## Building and Running

### Prerequisites

- Xcode (for iOS/macOS development)
- CMake 3.18 or later
- Google Test framework
- BabylonNative dependencies

### Build Instructions

1. **Navigate to the test directory:**
   ```bash
   cd /path/to/BabylonNative/Apps/ClipchampTests
   ```

2. **Create build directory:**
   ```bash
   mkdir build && cd build
   ```

3. **Configure with CMake:**
   ```bash
   cmake ..
   ```

4. **Build the tests:**
   ```bash
   make -j$(nproc)
   ```

### Running Tests

1. **Run all tests:**
   ```bash
   ./ClipchampBabylonNativeTests
   ```

2. **Run specific test suite:**
   ```bash
   ./ClipchampBabylonNativeTests --gtest_filter="ClipchampInitializationTest.*"
   ```

3. **Run with verbose output:**
   ```bash
   ./ClipchampBabylonNativeTests --gtest_verbose
   ```

4. **Generate test report:**
   ```bash
   ./ClipchampBabylonNativeTests --gtest_output=xml:test_results.xml
   ```

## Test Architecture

The test suite is designed to mirror Clipchamp's actual usage patterns:

### Mock Components

- **MockSuperfillDelegate**: Implements the BabylonNativeBridgeDelegate interface for testing Superfill interactions
- **Test Fixtures**: Set up and tear down BabylonNative components in the same order as Clipchamp's implementation

### Platform Considerations

- **iOS/macOS**: Uses Metal device creation and MTKView configuration
- **Cross-platform**: Includes fallback implementations for non-Apple platforms
- **Resource Management**: Proper cleanup for platform-specific resources

### Integration Points

The tests validate the exact integration points that Clipchamp uses:

1. **Graphics Configuration**: Metal device setup, viewport configuration
2. **JavaScript Environment**: Console, Window, XMLHttpRequest polyfills
3. **Plugin Integration**: NativeEngine, ExternalTexture plugins
4. **Superfill Communication**: Project loading, source management, export workflow

## Continuous Integration

These tests are designed to be run in CI/CD pipelines to validate:

- BabylonNative updates don't break Clipchamp's usage patterns
- Clipchamp's integration code maintains compatibility
- Performance characteristics remain within acceptable bounds
- Memory usage patterns are stable

## Contributing

When adding new tests:

1. Follow the existing test naming conventions
2. Use the established test fixtures for consistency
3. Include both positive and negative test cases
4. Document the specific Clipchamp usage pattern being tested
5. Ensure proper cleanup in test tear-down methods

## Troubleshooting

### Common Issues

1. **Metal device creation failures**: Ensure running on macOS/iOS with Metal support
2. **CMake configuration errors**: Verify BabylonNative dependencies are built
3. **Linking errors**: Check that all required BabylonNative libraries are included
4. **Runtime crashes**: Ensure proper initialization order in test fixtures

### Debug Tips

1. Use `--gtest_break_on_failure` to break on first test failure
2. Enable BabylonNative logging for detailed debug output
3. Use memory debugging tools to detect leaks
4. Check console output for BabylonNative error messages

## Related Documentation

- [Superfill-Babylon Architecture](../../docs/architecture/superfill-babylon.md)
- [BabylonNative Documentation](https://github.com/BabylonJS/BabylonNative)
- [Clipchamp Mobile App Architecture](../../docs/architecture/)

This test suite ensures that Clipchamp's video editing capabilities built on BabylonNative remain stable and performant across all supported platforms.
