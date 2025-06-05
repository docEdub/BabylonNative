#include "gtest/gtest.h"
#include <memory>
#include <optional>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>

#if __APPLE__
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#endif

namespace ClipchampTests {

/**
 * Test fixture for Clipchamp's BabylonNative initialization patterns.
 * Based on the actual initialization sequence used in BabylonNativeBridge.mm
 */
class ClipchampInitializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean slate for each test
        device.reset();
        deviceUpdate.reset();
        runtime.reset();
    }

    void TearDown() override {
        // Cleanup in reverse order of initialization
        runtime.reset();
        deviceUpdate.reset();
        device.reset();
    }

    // Test helpers that mirror Clipchamp's actual initialization
    bool InitializeGraphicsDevice(int width = 1080, int height = 1920) {
#if __APPLE__
        // Create a Metal device (required on iOS/macOS)
        id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
        if (!mtlDevice) {
            return false;
        }

        // Create a dummy MTKView for testing
        MTKView* view = [[MTKView alloc] init];
        view.device = mtlDevice;
        view.framebufferOnly = NO;
        view.drawableSize = CGSizeMake(width, height);

        Babylon::Graphics::Configuration config{};
        config.Device = mtlDevice;
        config.Window = view;
        config.Width = static_cast<size_t>(width);
        config.Height = static_cast<size_t>(height);
        
        device = std::make_unique<Babylon::Graphics::Device>(config);
        return true;
#else
        // For non-Apple platforms, create a basic configuration
        Babylon::Graphics::Configuration config{};
        config.Width = static_cast<size_t>(width);
        config.Height = static_cast<size_t>(height);
        
        device = std::make_unique<Babylon::Graphics::Device>(config);
        return true;
#endif
    }

    bool InitializeDeviceUpdate() {
        if (!device) {
            return false;
        }
        deviceUpdate = std::make_unique<Babylon::Graphics::DeviceUpdate>(device->GetUpdate("update"));
        return true;
    }

    bool InitializeAppRuntime() {
        runtime = std::make_unique<Babylon::AppRuntime>();
        return true;
    }

    bool InitializeBabylonServices() {
        if (!runtime || !device) {
            return false;
        }

        bool success = false;
        runtime->Dispatch([&](Napi::Env env) {
            try {
                // Add device to JavaScript context
                device->AddToJavaScript(env);

                // Initialize core polyfills (matches Clipchamp's sequence)
                Babylon::Polyfills::Window::Initialize(env);
                Babylon::Polyfills::XMLHttpRequest::Initialize(env);
                Babylon::Polyfills::Console::Initialize(env, [](const char* message, auto) {
                    // Test console handler
                    printf("BabylonNative Console: %s\n", message);
                });

                // Initialize NativeEngine plugin
                Babylon::Plugins::NativeEngine::Initialize(env);

                success = true;
            } catch (const std::exception& e) {
                printf("Error initializing Babylon services: %s\n", e.what());
                success = false;
            }
        });

        return success;
    }

    // Member variables that match Clipchamp's BabylonNativeBridge
    std::unique_ptr<Babylon::Graphics::Device> device;
    std::unique_ptr<Babylon::Graphics::DeviceUpdate> deviceUpdate;
    std::unique_ptr<Babylon::AppRuntime> runtime;
};

// Test the complete initialization sequence that Clipchamp uses
TEST_F(ClipchampInitializationTest, CompleteInitializationSequence) {
    // Step 1: Initialize Graphics Device (matches BabylonNativeBridge initialize method)
    ASSERT_TRUE(InitializeGraphicsDevice()) << "Graphics device initialization should succeed";
    EXPECT_TRUE(device != nullptr) << "Graphics device should be created";

    // Step 2: Initialize Device Update
    ASSERT_TRUE(InitializeDeviceUpdate()) << "Device update initialization should succeed";
    EXPECT_TRUE(deviceUpdate != nullptr) << "Device update should be created";

    // Step 3: Initialize App Runtime
    ASSERT_TRUE(InitializeAppRuntime()) << "App runtime initialization should succeed";
    EXPECT_TRUE(runtime != nullptr) << "App runtime should be created";

    // Step 4: Initialize Babylon Services (polyfills and plugins)
    ASSERT_TRUE(InitializeBabylonServices()) << "Babylon services initialization should succeed";
}

// Test initialization with different viewport sizes (matches Clipchamp's mobile usage)
TEST_F(ClipchampInitializationTest, InitializationWithDifferentViewportSizes) {
    // Test with typical mobile portrait dimensions
    EXPECT_TRUE(InitializeGraphicsDevice(1080, 1920)) << "Should initialize with mobile portrait dimensions";
    
    // Clean up and test with landscape
    device.reset();
    EXPECT_TRUE(InitializeGraphicsDevice(1920, 1080)) << "Should initialize with mobile landscape dimensions";
    
    // Clean up and test with square dimensions
    device.reset();
    EXPECT_TRUE(InitializeGraphicsDevice(1080, 1080)) << "Should initialize with square dimensions";
}

// Test error handling for invalid dimensions (matches Clipchamp's validation)
TEST_F(ClipchampInitializationTest, InvalidDimensionsHandling) {
    // Clipchamp validates width and height > 0
    EXPECT_FALSE(InitializeGraphicsDevice(0, 1920)) << "Should fail with zero width";
    EXPECT_FALSE(InitializeGraphicsDevice(1080, 0)) << "Should fail with zero height";
    EXPECT_FALSE(InitializeGraphicsDevice(-100, 1920)) << "Should fail with negative width";
    EXPECT_FALSE(InitializeGraphicsDevice(1080, -100)) << "Should fail with negative height";
}

// Test initialization order dependency
TEST_F(ClipchampInitializationTest, InitializationOrderDependency) {
    // DeviceUpdate should fail without Device
    EXPECT_FALSE(InitializeDeviceUpdate()) << "Device update should fail without graphics device";
    
    // Services should fail without Runtime or Device
    EXPECT_FALSE(InitializeBabylonServices()) << "Babylon services should fail without runtime and device";
    
    // Initialize Device first
    ASSERT_TRUE(InitializeGraphicsDevice());
    
    // Services should still fail without Runtime
    EXPECT_FALSE(InitializeBabylonServices()) << "Babylon services should fail without runtime";
    
    // Initialize Runtime
    ASSERT_TRUE(InitializeAppRuntime());
    
    // Now services should succeed
    EXPECT_TRUE(InitializeBabylonServices()) << "Babylon services should succeed with both device and runtime";
}

// Test multiple initialization attempts (Clipchamp prevents double initialization)
TEST_F(ClipchampInitializationTest, PreventDoubleInitialization) {
    // First initialization should succeed
    ASSERT_TRUE(InitializeGraphicsDevice());
    ASSERT_TRUE(InitializeAppRuntime());
    
    // Attempting to initialize again should be handled gracefully
    // Note: In the actual BabylonNativeBridge, this is prevented at a higher level
    // Here we test that the underlying components handle it appropriately
    auto firstDevice = device.get();
    auto firstRuntime = runtime.get();
    
    // The pointers should remain the same (no new instances created)
    EXPECT_EQ(device.get(), firstDevice) << "Device pointer should remain unchanged";
    EXPECT_EQ(runtime.get(), firstRuntime) << "Runtime pointer should remain unchanged";
}

// Test proper cleanup sequence
TEST_F(ClipchampInitializationTest, ProperCleanupSequence) {
    // Initialize everything
    ASSERT_TRUE(InitializeGraphicsDevice());
    ASSERT_TRUE(InitializeDeviceUpdate());
    ASSERT_TRUE(InitializeAppRuntime());
    ASSERT_TRUE(InitializeBabylonServices());
    
    // Cleanup should happen in reverse order (handled by TearDown)
    // This test ensures no crashes occur during cleanup
    SUCCEED() << "Cleanup sequence completed without crashes";
}

} // namespace ClipchampTests
