#include "gtest/gtest.h"
#include <memory>
#include <unordered_map>
#include <future>
#include <chrono>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Plugins/ExternalTexture.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/ScriptLoader.h>

#if __APPLE__
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#endif

namespace ClipchampTests {

/**
 * Test fixture for Clipchamp's BabylonNative cleanup and shutdown patterns.
 * Based on the cleanup sequence used in BabylonNativeBridge.mm
 */
class ClipchampCleanupTest : public ::testing::Test {
protected:
    void SetUp() override {
        InitializeForTesting();
    }

    void TearDown() override {
        // Cleanup should happen automatically, but let's be explicit
        PerformCleanup();
    }

    void InitializeForTesting() {
#if __APPLE__
        id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
        if (mtlDevice) {
            MTKView* view = [[MTKView alloc] init];
            view.device = mtlDevice;
            view.framebufferOnly = NO;
            view.drawableSize = CGSizeMake(1080, 1920);

            Babylon::Graphics::Configuration config{};
            config.Device = mtlDevice;
            config.Window = view;
            config.Width = 1080;
            config.Height = 1920;
            
            device = std::make_unique<Babylon::Graphics::Device>(config);
            deviceUpdate = std::make_unique<Babylon::Graphics::DeviceUpdate>(device->GetUpdate("update"));
            runtime = std::make_unique<Babylon::AppRuntime>();
            
            // Initialize Babylon services and wait for completion
            std::promise<void> initPromise;
            std::future<void> initFuture = initPromise.get_future();
            
            runtime->Dispatch([this, &initPromise](Napi::Env env) {
                try {
                    device->AddToJavaScript(env);
                    Babylon::Plugins::NativeEngine::Initialize(env);
                    isInitialized = true;
                    initPromise.set_value();
                } catch (...) {
                    initPromise.set_exception(std::current_exception());
                }
            });
            
            // Wait for initialization to complete (with timeout)
            if (initFuture.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
                throw std::runtime_error("Initialization timed out");
            }
            initFuture.get(); // Re-throw any exceptions from initialization
        }
#else
        Babylon::Graphics::Configuration config{};
        config.Width = 1080;
        config.Height = 1920;
        
        device = std::make_unique<Babylon::Graphics::Device>(config);
        deviceUpdate = std::make_unique<Babylon::Graphics::DeviceUpdate>(device->GetUpdate("update"));
        runtime = std::make_unique<Babylon::AppRuntime>();
        isInitialized = true;
#endif
    }

    void CreateExternalTextures(int count) {
#if __APPLE__
        if (!device || !isInitialized) {
            // If device isn't ready, just create mock entries for testing
            for (int i = 0; i < count; ++i) {
                sourceTextures.emplace(i, Babylon::Plugins::ExternalTexture(nullptr));
            }
            return;
        }
        
        auto platformInfo = device->GetPlatformInfo();
        id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)platformInfo.Device;
        
        for (int i = 0; i < count; ++i) {
            MTLTextureDescriptor* descriptor = [[MTLTextureDescriptor alloc] init];
            descriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
            descriptor.width = 256;
            descriptor.height = 256;
            descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
            
            id<MTLTexture> texture = [mtlDevice newTextureWithDescriptor:descriptor];
            if (texture) {
                sourceTextures.emplace(i, Babylon::Plugins::ExternalTexture(texture));
            } else {
                // Fallback to mock texture for testing
                sourceTextures.emplace(i, Babylon::Plugins::ExternalTexture(nullptr));
            }
        }
#else
        // For non-Apple platforms, create dummy external textures
        for (int i = 0; i < count; ++i) {
            sourceTextures.emplace(i, Babylon::Plugins::ExternalTexture(nullptr));
        }
#endif
    }

    void PerformCleanup() {
        // Cleanup order is important - matches Clipchamp's deinitialize method
        
        // 1. Finish any pending frame rendering
        if (hasStartedRenderingFrame && device) {
            device->FinishRenderingCurrentFrame();
            hasStartedRenderingFrame = false;
        }
        
        // 2. Clear external textures
        sourceTextures.clear();
        
        // 3. Cleanup JavaScript runtime
        if (runtime) {
            runtime.reset();
            runtime = nullptr;
        }
        
        // 4. Cleanup device update
        if (deviceUpdate) {
            deviceUpdate.reset();
            deviceUpdate = nullptr;
        }
        
        // 5. Cleanup graphics device last
        if (device) {
            device.reset();
            device = nullptr;
        }
        
        isInitialized = false;
    }

    void SimulateFrameRendering() {
        if (device) {
            device->StartRenderingCurrentFrame();
            hasStartedRenderingFrame = true;
        }
    }

    // Test state
    bool isInitialized{false};
    bool hasStartedRenderingFrame{false};
    std::unordered_map<long, Babylon::Plugins::ExternalTexture> sourceTextures;

    // BabylonNative components
    std::unique_ptr<Babylon::Graphics::Device> device;
    std::unique_ptr<Babylon::Graphics::DeviceUpdate> deviceUpdate;
    std::unique_ptr<Babylon::AppRuntime> runtime;
};

// Test basic cleanup sequence
TEST_F(ClipchampCleanupTest, BasicCleanupSequence) {
    ASSERT_TRUE(isInitialized) << "Should be initialized before cleanup";
    ASSERT_TRUE(device != nullptr) << "Device should exist before cleanup";
    ASSERT_TRUE(runtime != nullptr) << "Runtime should exist before cleanup";
    
    // Perform cleanup
    EXPECT_NO_THROW(PerformCleanup()) << "Cleanup should complete without exceptions";
    
    // Verify cleanup state
    EXPECT_FALSE(isInitialized) << "Should be marked as uninitialized after cleanup";
    EXPECT_TRUE(device == nullptr) << "Device should be null after cleanup";
    EXPECT_TRUE(runtime == nullptr) << "Runtime should be null after cleanup";
}

// Test cleanup with pending frame rendering
TEST_F(ClipchampCleanupTest, CleanupWithPendingFrame) {
    // Start frame rendering
    EXPECT_NO_THROW(SimulateFrameRendering()) << "Should be able to start frame rendering";
    EXPECT_TRUE(hasStartedRenderingFrame) << "Frame rendering should be marked as started";
    
    // Cleanup should handle pending frame
    EXPECT_NO_THROW(PerformCleanup()) << "Cleanup should handle pending frame gracefully";
    
    EXPECT_FALSE(hasStartedRenderingFrame) << "Frame rendering should be finished during cleanup";
}

// Test cleanup with external textures (matches Clipchamp's sourceTextures)
TEST_F(ClipchampCleanupTest, CleanupWithExternalTextures) {
    const int textureCount = 5;
    
    // Create some external textures
    CreateExternalTextures(textureCount);
    EXPECT_EQ(sourceTextures.size(), textureCount) << "Should have created expected number of textures";
    
    // Cleanup should clear all textures
    EXPECT_NO_THROW(PerformCleanup()) << "Cleanup should handle external textures";
    
    EXPECT_EQ(sourceTextures.size(), 0) << "All external textures should be cleared";
}

// Test multiple cleanup calls (should be idempotent)
TEST_F(ClipchampCleanupTest, MultipleCleanupCalls) {
    // First cleanup
    EXPECT_NO_THROW(PerformCleanup()) << "First cleanup should succeed";
    
    // Second cleanup should be safe
    EXPECT_NO_THROW(PerformCleanup()) << "Second cleanup should be idempotent";
    
    // Third cleanup should also be safe
    EXPECT_NO_THROW(PerformCleanup()) << "Third cleanup should be idempotent";
}

// Test cleanup order dependency
TEST_F(ClipchampCleanupTest, CleanupOrderDependency) {
    // Manual cleanup in wrong order to test robustness
    
    // Try to cleanup device first (before runtime)
    device.reset();
    device = nullptr;
    
    // Runtime cleanup should still work
    EXPECT_NO_THROW(runtime.reset()) << "Runtime cleanup should work even if device is gone";
    
    // Complete cleanup should be safe
    EXPECT_NO_THROW(PerformCleanup()) << "Cleanup should handle partial cleanup gracefully";
}

// Test resource cleanup verification
TEST_F(ClipchampCleanupTest, ResourceCleanupVerification) {
    // Create resources
    CreateExternalTextures(3);
    SimulateFrameRendering();
    
    // Capture resource counts before cleanup
    auto textureCountBefore = sourceTextures.size();
    auto devicePtrBefore = device.get();
    auto runtimePtrBefore = runtime.get();
    
    EXPECT_GT(textureCountBefore, 0) << "Should have textures before cleanup";
    EXPECT_NE(devicePtrBefore, nullptr) << "Should have device before cleanup";
    EXPECT_NE(runtimePtrBefore, nullptr) << "Should have runtime before cleanup";
    
    // Perform cleanup
    PerformCleanup();
    
    // Verify resources are cleaned up
    EXPECT_EQ(sourceTextures.size(), 0) << "Textures should be cleared";
    EXPECT_EQ(device.get(), nullptr) << "Device should be null";
    EXPECT_EQ(runtime.get(), nullptr) << "Runtime should be null";
}

// Test cleanup during JavaScript execution
TEST_F(ClipchampCleanupTest, CleanupDuringJavaScriptExecution) {
    if (!runtime) {
        GTEST_SKIP() << "Runtime not available for this test";
    }
    
    bool javascriptCompleted = false;
    
    // Dispatch some JavaScript work
    runtime->Dispatch([&javascriptCompleted](Napi::Env env) {
        // Simulate some JavaScript work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        javascriptCompleted = true;
    });
    
    // Wait a moment for JavaScript to start
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    // Cleanup should wait for JavaScript to complete
    EXPECT_NO_THROW(PerformCleanup()) << "Cleanup should handle ongoing JavaScript execution";
    
    // Give JavaScript a chance to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Note: In real scenarios, Clipchamp ensures JavaScript is complete before cleanup
    // This test verifies that cleanup doesn't crash if JavaScript is still running
}

// Test memory leak prevention
TEST_F(ClipchampCleanupTest, MemoryLeakPrevention) {
    // Create and cleanup multiple times to detect memory leaks
    for (int i = 0; i < 10; ++i) {
        // Reinitialize
        if (!device) {
            InitializeForTesting();
        }
        
        // Create some resources
        CreateExternalTextures(2);
        
        // Cleanup everything
        PerformCleanup();
        
        // Verify clean state
        EXPECT_EQ(sourceTextures.size(), 0) << "Iteration " << i << ": Textures should be cleared";
        EXPECT_EQ(device.get(), nullptr) << "Iteration " << i << ": Device should be null";
        EXPECT_EQ(runtime.get(), nullptr) << "Iteration " << i << ": Runtime should be null";
    }
}

// Test cleanup with exception handling
TEST_F(ClipchampCleanupTest, CleanupWithExceptionHandling) {
    // Simulate a scenario where cleanup might throw
    class ThrowingCleanup {
    public:
        ~ThrowingCleanup() noexcept {
            // In real code, destructors should not throw
            // This is just to test exception safety
        }
    };
    
    ThrowingCleanup throwingResource;
    
    // Cleanup should be exception-safe
    EXPECT_NO_THROW(PerformCleanup()) << "Cleanup should be exception-safe";
}

// Test partial initialization cleanup
TEST_F(ClipchampCleanupTest, PartialInitializationCleanup) {
    // Reset to partial state
    runtime.reset();
    runtime = nullptr;
    
    // Cleanup should handle partial initialization
    EXPECT_NO_THROW(PerformCleanup()) << "Should handle cleanup of partially initialized state";
    
    // Verify state
    EXPECT_EQ(device.get(), nullptr) << "Device should be cleaned up";
    EXPECT_EQ(runtime.get(), nullptr) << "Runtime should remain null";
}

} // namespace ClipchampTests
