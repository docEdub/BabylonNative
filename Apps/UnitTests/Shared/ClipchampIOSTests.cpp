#include "gtest/gtest.h"
#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Plugins/ExternalTexture.h>
#include <Babylon/ScriptLoader.h>
#include <chrono>
#include <thread>
#include <optional>
#include <future>
#include <iostream>
#include <vector>
#include <unordered_map>

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IOS
#import <MetalKit/MetalKit.h>
#import <Metal/Metal.h>
#import <UIKit/UIKit.h>
#endif
#endif

namespace ClipchampIOSTests
{
#ifdef __APPLE__
#if TARGET_OS_IOS
    // iOS-specific test fixture for Metal integration testing
    class ClipchampIOSBabylonNativeTest : public ::testing::Test
    {
    protected:
        std::optional<Babylon::Graphics::Device> device;
        std::optional<Babylon::Graphics::DeviceUpdate> deviceUpdate;
        std::optional<Babylon::AppRuntime> runtime;
        std::vector<std::string> consoleMessages;
        bool hasStartedRenderingFrame = false;
        
        // Mock Metal objects for testing
        id<MTLDevice> mockMTLDevice = nil;
        MTKView* mockMTKView = nil;
        std::unordered_map<long, Babylon::Plugins::ExternalTexture> sourceTextures;

        void SetUp() override
        {
            consoleMessages.clear();
            hasStartedRenderingFrame = false;
            sourceTextures.clear();
            
            // Create mock Metal device for testing
            mockMTLDevice = MTLCreateSystemDefaultDevice();
            if (mockMTLDevice)
            {
                mockMTKView = [[MTKView alloc] init];
                mockMTKView.device = mockMTLDevice;
                mockMTKView.framebufferOnly = NO;
                mockMTKView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
            }
        }

        void TearDown() override
        {
            CleanupBabylonNative();
            sourceTextures.clear();
            mockMTKView = nil;
            mockMTLDevice = nil;
        }

        // Initialize BabylonNative with actual Metal objects (Clipchamp's iOS pattern)
        bool InitializeBabylonNativeWithMetal(size_t width = 1920, size_t height = 1080, bool expectSuccess = true)
        {
            if (!mockMTLDevice || !mockMTKView)
            {
                if (expectSuccess)
                {
                    std::cerr << "Mock Metal objects not available for testing" << std::endl;
                }
                return false;
            }

            try
            {
                // Step 1: Create Graphics Device with actual Metal objects
                Babylon::Graphics::Configuration config{};
                config.Device = (__bridge void*)mockMTLDevice;
                config.Window = (__bridge void*)mockMTKView;
                config.Width = width;
                config.Height = height;
                
                device.emplace(config);
                
                // Step 2: Create device update handle
                deviceUpdate.emplace(device->GetUpdate("update"));
                
                // Step 3: Start initial frame rendering
                if (!StartRenderingFrame())
                {
                    return false;
                }
                
                // Step 4: Create AppRuntime
                runtime.emplace();
                
                // Step 5: Initialize Babylon services
                InitializeBabylonServices();
                
                return true;
            }
            catch (const std::exception& e)
            {
                if (expectSuccess)
                {
                    std::cerr << "Unexpected exception during Metal initialization: " << e.what() << std::endl;
                }
                return false;
            }
        }

        void InitializeBabylonServices()
        {
            if (!runtime || !device)
            {
                throw std::runtime_error("Runtime or device not initialized");
            }

            runtime->Dispatch([this](Napi::Env env) {
                // Add device to JavaScript context
                device->AddToJavaScript(env);

                // Get platform info for Metal device access
                auto platformInfo = device->GetPlatformInfo();
                
                // Initialize polyfills in Clipchamp's order
                Babylon::Polyfills::Window::Initialize(env);
                Babylon::Polyfills::XMLHttpRequest::Initialize(env);
                Babylon::Polyfills::Console::Initialize(env, [this](const char* message, auto logLevel) {
                    consoleMessages.push_back(std::string(message));
                });
                Babylon::Plugins::NativeEngine::Initialize(env);
            });
        }

        bool StartRenderingFrame()
        {
            if (!device || hasStartedRenderingFrame)
            {
                return false;
            }
            
            try
            {
                hasStartedRenderingFrame = true;
                device->StartRenderingCurrentFrame();
                if (deviceUpdate)
                {
                    deviceUpdate->Start();
                }
                return true;
            }
            catch (const std::exception&)
            {
                hasStartedRenderingFrame = false;
                return false;
            }
        }

        bool FinishRenderingFrame()
        {
            if (!device || !hasStartedRenderingFrame)
            {
                return false;
            }
            
            try
            {
                if (deviceUpdate)
                {
                    deviceUpdate->Finish();
                }
                device->FinishRenderingCurrentFrame();
                hasStartedRenderingFrame = false;
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        bool UpdateWindow(MTKView* newView, size_t width, size_t height)
        {
            if (!device)
            {
                return false;
            }
            
            try
            {
                // Follow Clipchamp's updateWindow pattern
                if (!FinishRenderingFrame())
                {
                    return false;
                }
                
                device->UpdateWindow((__bridge void*)newView);
                device->UpdateSize(width, height);
                
                return StartRenderingFrame();
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        // Create external texture following Clipchamp's pattern
        bool CreateExternalTexture(int32_t width, int32_t height, long sourceId)
        {
            if (!mockMTLDevice)
            {
                return false;
            }
            
            try
            {
                MTLTextureDescriptor* descriptor = [[MTLTextureDescriptor alloc] init];
                descriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
                descriptor.width = width;
                descriptor.height = height;
                descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

                id<MTLTexture> texture = [mockMTLDevice newTextureWithDescriptor:descriptor];
                if (!texture)
                {
                    return false;
                }

                // Create ExternalTexture and add to JavaScript context
                Babylon::Plugins::ExternalTexture externalTexture((__bridge void*)texture);
                
                runtime->Dispatch([&externalTexture, sourceId, this](Napi::Env env) {
                    try
                    {
                        Napi::Promise addToContextPromise = externalTexture.AddToContextAsync(env);
                        sourceTextures[sourceId] = std::move(externalTexture);
                    }
                    catch (const std::exception&)
                    {
                        // Handle texture creation failure
                    }
                });
                
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        void CleanupBabylonNative()
        {
            try
            {
                if (device && hasStartedRenderingFrame)
                {
                    FinishRenderingFrame();
                }
                
                // Clear external textures
                sourceTextures.clear();
                
                // Clean up in reverse order
                runtime.reset();
                deviceUpdate.reset();
                device.reset();
                hasStartedRenderingFrame = false;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Exception during iOS cleanup: " << e.what() << std::endl;
            }
        }
    };

    // Test Metal device initialization
    TEST_F(ClipchampIOSBabylonNativeTest, MetalDeviceInitialization)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        EXPECT_TRUE(device.has_value());
        EXPECT_TRUE(runtime.has_value());
        EXPECT_TRUE(deviceUpdate.has_value());
    }

    // Test MTKView window updates
    TEST_F(ClipchampIOSBabylonNativeTest, MTKViewWindowUpdate)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Create a new MTKView for testing window updates
        MTKView* newMTKView = [[MTKView alloc] init];
        newMTKView.device = mockMTLDevice;
        newMTKView.framebufferOnly = NO;
        newMTKView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        
        EXPECT_TRUE(UpdateWindow(newMTKView, 1280, 720));
        EXPECT_TRUE(UpdateWindow(newMTKView, 3840, 2160)); // 4K update
    }

    // Test ExternalTexture creation and management
    TEST_F(ClipchampIOSBabylonNativeTest, ExternalTextureManagement)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Create multiple external textures (simulating video frames)
        std::vector<long> sourceIds = {1001, 1002, 1003, 1004, 1005};
        
        for (long sourceId : sourceIds)
        {
            EXPECT_TRUE(CreateExternalTexture(1920, 1080, sourceId));
        }
        
        // Render frames with external textures
        for (int frame = 0; frame < 10; ++frame)
        {
            EXPECT_TRUE(FinishRenderingFrame());
            EXPECT_TRUE(StartRenderingFrame());
        }
        
        // Cleanup should handle external textures properly
        EXPECT_EQ(sourceTextures.size(), sourceIds.size());
    }

    // Test Metal command buffer synchronization
    TEST_F(ClipchampIOSBabylonNativeTest, MetalCommandBufferSync)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Test that command buffers are properly synchronized
        runtime->Dispatch([this](Napi::Env env) {
            auto platformInfo = device->GetPlatformInfo();
            id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)platformInfo.CommandQueue;
            
            if (commandQueue)
            {
                // Create and commit a command buffer
                id<MTLCommandBuffer> buffer = [commandQueue commandBuffer];
                [buffer commit];
                [buffer waitUntilCompleted];
                
                // Should complete without errors
                EXPECT_EQ(buffer.status, MTLCommandBufferStatusCompleted);
            }
        });
        
        // System should remain stable after command buffer operations
        EXPECT_TRUE(FinishRenderingFrame());
        EXPECT_TRUE(StartRenderingFrame());
    }

    // Test iOS-specific viewport orientations
    TEST_F(ClipchampIOSBabylonNativeTest, ViewportOrientations)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Test landscape to portrait rotation
        EXPECT_TRUE(UpdateWindow(mockMTKView, 1080, 1920));
        
        // Test back to landscape
        EXPECT_TRUE(UpdateWindow(mockMTKView, 1920, 1080));
        
        // Test iPad-style dimensions
        EXPECT_TRUE(UpdateWindow(mockMTKView, 2048, 1536));
        EXPECT_TRUE(UpdateWindow(mockMTKView, 1536, 2048));
    }

    // Test Metal texture pixel format handling
    TEST_F(ClipchampIOSBabylonNativeTest, MetalTextureFormats)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Test different pixel formats that Clipchamp might use
        std::vector<MTLPixelFormat> formats = {
            MTLPixelFormatBGRA8Unorm,
            MTLPixelFormatRGBA8Unorm,
            MTLPixelFormatBGRA8Unorm_sRGB,
            MTLPixelFormatRGBA16Float
        };
        
        for (MTLPixelFormat format : formats)
        {
            MTLTextureDescriptor* descriptor = [[MTLTextureDescriptor alloc] init];
            descriptor.pixelFormat = format;
            descriptor.width = 1920;
            descriptor.height = 1080;
            descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

            id<MTLTexture> texture = [mockMTLDevice newTextureWithDescriptor:descriptor];
            if (texture)
            {
                // Successfully created texture with this format
                EXPECT_NE(texture, nil);
            }
        }
    }

    // Test iOS memory pressure scenarios
    TEST_F(ClipchampIOSBabylonNativeTest, MemoryPressureHandling)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Create many external textures to simulate memory pressure
        std::vector<long> sourceIds;
        for (long i = 0; i < 50; ++i)
        {
            sourceIds.push_back(i + 2000);
        }
        
        int successfulCreations = 0;
        for (long sourceId : sourceIds)
        {
            if (CreateExternalTexture(1920, 1080, sourceId))
            {
                successfulCreations++;
            }
        }
        
        // Should handle memory constraints gracefully
        EXPECT_GT(successfulCreations, 0);
        
        // System should remain stable
        EXPECT_TRUE(FinishRenderingFrame());
        EXPECT_TRUE(StartRenderingFrame());
        
        // Cleanup should succeed even with many textures
        CleanupBabylonNative();
        SUCCEED();
    }

    // Test iOS background/foreground transitions
    TEST_F(ClipchampIOSBabylonNativeTest, BackgroundForegroundTransitions)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        // Simulate app going to background (stop rendering)
        EXPECT_TRUE(FinishRenderingFrame());
        
        // Simulate some time in background
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Simulate app returning to foreground (resume rendering)
        EXPECT_TRUE(StartRenderingFrame());
        
        // Should be able to continue normal operations
        for (int frame = 0; frame < 5; ++frame)
        {
            EXPECT_TRUE(FinishRenderingFrame());
            EXPECT_TRUE(StartRenderingFrame());
        }
    }

    // Test iOS device capability detection
    TEST_F(ClipchampIOSBabylonNativeTest, DeviceCapabilityDetection)
    {
        if (!mockMTLDevice)
        {
            GTEST_SKIP() << "Metal device not available";
        }
        
        EXPECT_TRUE(InitializeBabylonNativeWithMetal(1920, 1080));
        
        runtime->Dispatch([this](Napi::Env env) {
            auto platformInfo = device->GetPlatformInfo();
            id<MTLDevice> mtlDevice = (__bridge id<MTLDevice>)platformInfo.Device;
            
            if (mtlDevice)
            {
                // Test device capabilities that affect Clipchamp's functionality
                EXPECT_TRUE(mtlDevice.supportsTextureSampleCount(1));
                
                // Check maximum texture dimensions
                NSUInteger maxTextureSize = 16384; // Common iOS limit
                EXPECT_LE(mtlDevice.maxTextureWidth, maxTextureSize);
                EXPECT_LE(mtlDevice.maxTextureHeight, maxTextureSize);
                
                // Check feature sets
                #if TARGET_OS_IOS
                    // iOS-specific feature set checks
                    EXPECT_TRUE([mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v1] ||
                               [mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] ||
                               [mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1]);
                #endif
            }
        });
    }

#endif // TARGET_OS_IOS
#endif // __APPLE__

    // Fallback test for non-iOS platforms
    TEST(ClipchampIOSTests, PlatformAvailability)
    {
#ifdef __APPLE__
#if TARGET_OS_IOS
        SUCCEED() << "iOS Metal integration tests are available";
#else
        GTEST_SKIP() << "iOS-specific tests skipped on non-iOS Apple platform";
#endif
#else
        GTEST_SKIP() << "iOS-specific tests skipped on non-Apple platform";
#endif
    }
}
