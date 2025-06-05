# Clipchamp BabylonNative Tests

This directory contains comprehensive tests designed specifically for Clipchamp's usage patterns of BabylonNative on iOS. These tests cover the critical scenarios that Clipchamp encounters in production.

## Overview

The Clipchamp BabylonNative tests are designed to validate:

1. **BabylonNative Initialization Patterns** - Following Clipchamp's specific initialization sequence
2. **iOS Metal Integration** - Testing MTKView, Metal device, and ExternalTexture management
3. **JavaScript Runtime Integration** - Validating Superfill compositor communication patterns
4. **Lifecycle Management** - Testing proper initialization, resize, and cleanup procedures
5. **Error Handling** - Ensuring robust error recovery and resource management

## Test Files

### Core Test Suites

1. **`ClipchampTests.cpp`** - Platform-agnostic BabylonNative lifecycle tests
   - Initialization with valid/invalid parameters
   - Viewport resizing functionality
   - Frame rendering lifecycle management
   - Resource cleanup and memory management
   - Error condition handling

2. **`ClipchampIOSTests.cpp`** - iOS-specific Metal integration tests
   - MTKView window updates and orientation changes
   - ExternalTexture creation and management
   - Metal command buffer synchronization
   - iOS memory pressure scenarios
   - Background/foreground transitions
   - Device capability detection

3. **`ClipchampJavaScriptTests.cpp`** - JavaScript runtime integration tests
   - Superfill compositor function caching (performance optimization)
   - Project loading and track item management
   - Playback control (play/pause/seek)
   - Filter enumeration and application
   - Export functionality testing
   - Error handling in JavaScript context

### Supporting Files

4. **`build_clipchamp_tests.sh`** - Build script for iOS testing
5. **`ClipchampCMakeLists.txt`** - iOS-compatible CMake configuration
6. **`CLIPCHAMP_TESTS.md`** - This documentation file

## Key Test Scenarios

### Initialization Sequence Testing

The tests validate Clipchamp's specific BabylonNative initialization order:

1. `Babylon::Graphics::Device` creation with Metal objects
2. `Babylon::Graphics::DeviceUpdate` handle creation
3. Frame rendering initialization (`StartRenderingCurrentFrame`)
4. `Babylon::AppRuntime` creation
5. Polyfills initialization (Window, XMLHttpRequest, Console, NativeEngine)
6. Script loading (Superfill compositor)
7. Function reference caching for performance

### iOS Metal Integration

- **MTKView Management**: Testing view updates, window changes, and orientation handling
- **ExternalTexture Lifecycle**: Creation, JavaScript context integration, and cleanup
- **Metal Device Capabilities**: Feature set detection and texture format support
- **Command Buffer Synchronization**: Proper frame timing and resource management

### JavaScript Runtime Patterns

- **Function Reference Caching**: Clipchamp's performance optimization for frequently called functions
- **Superfill Compositor API**: Testing all the JavaScript functions that Clipchamp's bridge exposes
- **Error Recovery**: Ensuring the system remains stable after JavaScript exceptions
- **Concurrent Operations**: Testing thread-safe operations between native and JavaScript contexts

## Building and Running Tests

### Prerequisites

- Xcode 14.0 or later
- iOS SDK 15.0 or later
- CMake 3.21 or later
- BabylonNative dependencies (automatically fetched)

### Quick Build

```bash
# Navigate to the test directory
cd /path/to/BabylonNative/Apps/UnitTests

# Run the build script
./build_clipchamp_tests.sh
```

### Manual Build Process

```bash
# Create build directory
mkdir -p ../../Build/ClipchampTests
cd ../../Build/ClipchampTests

# Configure for iOS Simulator
cmake ../.. \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

# Build the project
xcodebuild \
    -project BabylonNative.xcodeproj \
    -scheme ALL_BUILD \
    -configuration Debug \
    -sdk iphonesimulator \
    build
```

### Running Tests

Due to the iOS-specific nature of these tests, they should be run in one of the following ways:

1. **Xcode Simulator**: Open the generated Xcode project and run tests in iOS Simulator
2. **Device Testing**: Deploy to actual iOS device for full Metal functionality testing
3. **CI Integration**: Use Xcode Cloud or similar CI systems with iOS simulator support

## Test Categories

### 1. Initialization Tests

- `InitializeWithValidParameters`: Basic initialization with standard dimensions
- `InitializeWithInvalidDimensions`: Error handling for invalid viewport sizes
- `PreventMultipleInitialization`: Ensures singleton behavior

### 2. Lifecycle Tests

- `ViewportResizing`: Testing dynamic resolution changes during runtime
- `FrameRenderingLifecycle`: Proper frame start/finish sequence
- `ProperCleanup`: Resource cleanup and reinitialization capability
- `LifecycleStressTesting`: Multiple init/deinit cycles

### 3. iOS-Specific Tests

- `MetalDeviceInitialization`: Metal device and MTKView setup
- `MTKViewWindowUpdate`: Dynamic view updates and orientation changes
- `ExternalTextureManagement`: Video frame texture handling
- `MetalCommandBufferSync`: Command queue synchronization
- `ViewportOrientations`: Portrait/landscape transitions
- `MemoryPressureHandling`: Resource management under constraints

### 4. JavaScript Integration Tests

- `JavaScriptEnvironmentInitialization`: Basic runtime setup
- `SuperfillScriptLoading`: Compositor script initialization
- `FunctionReferenceCaching`: Performance optimization validation
- `ProjectLoading`: Project JSON parsing and setup
- `TrackItemTransformUpdates`: Video editing operations
- `PlaybackControl`: Play/pause/seek functionality
- `FilterEnumeration`: Video effect management
- `ExportFunctionality`: Video export pipeline

### 5. Error Handling Tests

- `FrameOperationErrorHandling`: Invalid frame operations
- `JavaScriptErrorHandling`: Exception recovery
- `ErrorRecovery`: System stability after errors
- `ConcurrentOperationsSafety`: Thread safety validation

## Integration with Clipchamp's CI/CD

These tests are designed to be integrated into Clipchamp's existing CI/CD pipeline:

1. **Branch Protection**: Run tests on all PRs affecting BabylonNative integration
2. **Release Validation**: Full test suite execution before releases
3. **Performance Monitoring**: Track test execution times for performance regressions
4. **Device Matrix Testing**: Run on multiple iOS versions and devices

## Troubleshooting

### Common Issues

1. **Metal Dependencies**: Tests require iOS Simulator or device with Metal support
2. **JavaScript Errors**: Some tests may fail if Superfill compositor scripts are unavailable
3. **Memory Issues**: Large texture tests may fail on memory-constrained simulators
4. **Threading Issues**: Concurrent tests may show platform-specific behavior

### Debug Tips

1. Enable render logging in tests for detailed Metal operation tracking
2. Use Xcode Instruments for memory profiling during stress tests
3. Check console output for JavaScript error messages
4. Verify Metal device capabilities on target platform

## Contributing

When adding new tests:

1. Follow the existing test patterns and naming conventions
2. Add comprehensive documentation for new test scenarios
3. Ensure tests are deterministic and don't rely on external resources
4. Test on both simulator and device when possible
5. Update this documentation with new test descriptions

## Performance Considerations

- Tests are designed to run quickly for CI integration
- Large texture operations are limited to prevent memory issues
- Concurrent operations are tested but limited to prevent race conditions
- JavaScript operations use timeouts to prevent hanging

## Future Enhancements

Potential areas for expansion:

1. **Performance Benchmarking**: Automated performance regression testing
2. **Memory Leak Detection**: Automated memory profiling in CI
3. **Device-Specific Testing**: Tests for different iOS device capabilities
4. **Export Quality Validation**: Video output quality verification
5. **Network Integration**: Testing network-dependent operations

---

For questions or issues with these tests, please refer to the Clipchamp development team or create an issue in the project repository.
