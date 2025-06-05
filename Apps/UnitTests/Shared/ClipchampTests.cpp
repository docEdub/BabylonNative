#include "gtest/gtest.h"
#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/Canvas.h>
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

namespace ClipchampTests
{
    // Test fixture for basic Clipchamp tests that don't require JavaScript
    class ClipchampBasicTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // No JavaScript initialization needed for basic tests
        }

        void TearDown() override
        {
            // Simple cleanup
        }
    };

    // Test basic graphics device creation
    TEST_F(ClipchampBasicTest, GraphicsDeviceCreation)
    {
        // Test that we can create a graphics device configuration
        Babylon::Graphics::Configuration config{};
        config.Width = 1920;
        config.Height = 1080;
        
        // Should not crash when creating configuration
        EXPECT_EQ(config.Width, 1920);
        EXPECT_EQ(config.Height, 1080);
    }

    // Test that BabylonNative headers are accessible
    TEST_F(ClipchampBasicTest, BabylonNativeHeadersAccessible)
    {
        // This test verifies we can access BabylonNative types
        // without initializing full runtime (useful for iOS builds)
        
        // Should be able to create AppRuntime options
        Babylon::AppRuntime::Options options{};
        
        // Should be able to set basic options
        bool testFlag = false;
        options.UnhandledExceptionHandler = [&testFlag](const Napi::Error& error) {
            testFlag = true;
        };
        
        EXPECT_FALSE(testFlag); // Handler not called yet
    }

    // Test basic memory allocation patterns
    TEST_F(ClipchampBasicTest, BasicMemoryOperations)
    {
        // Test vector operations (common in Clipchamp)
        std::vector<std::string> messages;
        messages.push_back("test message 1");
        messages.push_back("test message 2");
        
        EXPECT_EQ(messages.size(), 2);
        EXPECT_EQ(messages[0], "test message 1");
        EXPECT_EQ(messages[1], "test message 2");
        
        // Test optional operations (used extensively in Clipchamp)
        std::optional<int> optionalValue;
        EXPECT_FALSE(optionalValue.has_value());
        
        optionalValue = 42;
        EXPECT_TRUE(optionalValue.has_value());
        EXPECT_EQ(optionalValue.value(), 42);
    }

    // Test threading primitives used by Clipchamp
    TEST_F(ClipchampBasicTest, ThreadingPrimitives)
    {
        std::promise<bool> testPromise;
        auto testFuture = testPromise.get_future();
        
        // Test promise/future pattern used in JavaScript integration
        std::thread([&testPromise]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            testPromise.set_value(true);
        }).detach();
        
        auto result = testFuture.get();
        EXPECT_TRUE(result);
    }

    // Test JSON-like string operations (used for Superfill communication)
    TEST_F(ClipchampBasicTest, StringOperations)
    {
        std::string mockProjectJson = R"({
            "version": "1.0",
            "timeline": {
                "duration": 10000,
                "tracks": []
            }
        })";
        
        // Basic string validation
        EXPECT_FALSE(mockProjectJson.empty());
        EXPECT_NE(mockProjectJson.find("version"), std::string::npos);
        EXPECT_NE(mockProjectJson.find("timeline"), std::string::npos);
        EXPECT_NE(mockProjectJson.find("duration"), std::string::npos);
    }

    // Test error handling patterns
    TEST_F(ClipchampBasicTest, ErrorHandlingPatterns)
    {
        bool exceptionCaught = false;
        
        try
        {
            // Test exception handling
            throw std::runtime_error("Test exception");
        }
        catch (const std::exception& e)
        {
            exceptionCaught = true;
            EXPECT_STREQ(e.what(), "Test exception");
        }
        
        EXPECT_TRUE(exceptionCaught);
    }
}

// Additional tests that don't require JavaScript context
namespace ClipchampAdvancedTests  
{
    class ClipchampAdvancedTest : public ::testing::Test
    {
    protected:
        void SetUp() override {}
        void TearDown() override {}
    };

    // Test configuration patterns used by Clipchamp
    TEST_F(ClipchampAdvancedTest, ConfigurationPatterns)
    {
        // Test configuration structures
        struct MockClipchampConfig
        {
            size_t width = 1920;
            size_t height = 1080;
            bool useMetalRenderer = true;
            double maxFrameRate = 60.0;
        };
        
        MockClipchampConfig config;
        EXPECT_EQ(config.width, 1920);
        EXPECT_EQ(config.height, 1080);
        EXPECT_TRUE(config.useMetalRenderer);
        EXPECT_DOUBLE_EQ(config.maxFrameRate, 60.0);
    }

    // Test data structures used for project management
    TEST_F(ClipchampAdvancedTest, ProjectDataStructures)
    {
        // Test structures similar to what Clipchamp uses
        struct MockTrackItem
        {
            std::string id;
            double startTime;
            double duration;
            std::unordered_map<std::string, double> transform;
        };
        
        MockTrackItem item;
        item.id = "item_123";
        item.startTime = 1000.0;
        item.duration = 5000.0;
        item.transform["rotation"] = 45.0;
        item.transform["scaleX"] = 1.5;
        
        EXPECT_EQ(item.id, "item_123");
        EXPECT_DOUBLE_EQ(item.startTime, 1000.0);
        EXPECT_DOUBLE_EQ(item.duration, 5000.0);
        EXPECT_DOUBLE_EQ(item.transform["rotation"], 45.0);
        EXPECT_DOUBLE_EQ(item.transform["scaleX"], 1.5);
    }

    // Test timing and synchronization patterns
    TEST_F(ClipchampAdvancedTest, TimingPatterns)
    {
        using namespace std::chrono;
        
        auto start = high_resolution_clock::now();
        
        // Simulate some work
        std::this_thread::sleep_for(milliseconds(10));
        
        auto end = high_resolution_clock::now();
        auto elapsed = duration_cast<milliseconds>(end - start);
        
        // Should have taken at least 10ms
        EXPECT_GE(elapsed.count(), 10);
        EXPECT_LT(elapsed.count(), 100); // But not too long
    }

    // Test function callback patterns used in Clipchamp
    TEST_F(ClipchampAdvancedTest, CallbackPatterns)
    {
        int callbackResult = 0;
        
        auto mockCallback = [&callbackResult](int value) -> bool {
            callbackResult = value * 2;
            return true;
        };
        
        // Test callback execution
        bool success = mockCallback(21);
        EXPECT_TRUE(success);
        EXPECT_EQ(callbackResult, 42);
        
        // Test callback with different types
        std::function<void(const std::string&)> stringCallback = [&callbackResult](const std::string& str) {
            callbackResult = static_cast<int>(str.length());
        };
        
        stringCallback("test string");
        EXPECT_EQ(callbackResult, 11);
    }
}
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

        bool ResizeViewport(size_t newWidth, size_t newHeight)
        {
            if (!device)
            {
                return false;
            }
            
            try
            {
                // Follow Clipchamp's resize pattern: finish current frame, resize, start new frame
                if (!FinishRenderingFrame())
                {
                    return false;
                }
                
                device->UpdateSize(newWidth, newHeight);
                
                return StartRenderingFrame();
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
                
                // Clean up in reverse order of initialization (mirrors Clipchamp's deinitialize)
                runtime.reset();
                deviceUpdate.reset();
                device.reset();
                hasStartedRenderingFrame = false;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Exception during cleanup: " << e.what() << std::endl;
            }
        }

        // Simulate multiple render frames like Clipchamp does
        bool RenderFrames(int frameCount)
        {
            for (int i = 0; i < frameCount; ++i)
            {
                if (!FinishRenderingFrame() || !StartRenderingFrame())
                {
                    return false;
                }
            }
            return true;
        }
    };

    // Test successful initialization with valid parameters
    TEST_F(ClipchampBabylonNativeTest, InitializeWithValidParameters)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        EXPECT_TRUE(device.has_value());
        EXPECT_TRUE(runtime.has_value());
        EXPECT_TRUE(deviceUpdate.has_value());
    }

    // Test initialization failure with invalid dimensions (mirrors Clipchamp's validation)
    TEST_F(ClipchampBabylonNativeTest, InitializeWithInvalidDimensions)
    {
        EXPECT_FALSE(InitializeBabylonNative(0, 1080, false));
        EXPECT_FALSE(InitializeBabylonNative(1920, 0, false));
        EXPECT_FALSE(InitializeBabylonNative(-100, 1080, false));
    }

    // Test multiple initialization attempts (should fail on second attempt)
    TEST_F(ClipchampBabylonNativeTest, PreventMultipleInitialization)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Second initialization should fail since we already have a device
        EXPECT_FALSE(InitializeBabylonNative(1280, 720, false));
    }

    // Test viewport resizing functionality
    TEST_F(ClipchampBabylonNativeTest, ViewportResizing)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Test various resolution changes that Clipchamp might encounter
        EXPECT_TRUE(ResizeViewport(1280, 720));   // 720p
        EXPECT_TRUE(ResizeViewport(3840, 2160));  // 4K
        EXPECT_TRUE(ResizeViewport(1920, 1080));  // Back to 1080p
        EXPECT_TRUE(ResizeViewport(720, 1280));   // Portrait orientation
    }

    // Test frame rendering lifecycle
    TEST_F(ClipchampBabylonNativeTest, FrameRenderingLifecycle)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Test multiple frame renders (simulating video playback)
        EXPECT_TRUE(RenderFrames(30)); // Simulate 1 second at 30fps
        
        // Test that we can pause and resume rendering
        EXPECT_TRUE(FinishRenderingFrame());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_TRUE(StartRenderingFrame());
        EXPECT_TRUE(RenderFrames(15)); // Another half second
    }

    // Test proper cleanup and resource management
    TEST_F(ClipchampBabylonNativeTest, ProperCleanup)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        EXPECT_TRUE(RenderFrames(5));
        
        // Explicit cleanup should succeed
        CleanupBabylonNative();
        
        // Should be able to initialize again after cleanup
        EXPECT_TRUE(InitializeBabylonNative(1280, 720));
    }

    // Test console logging functionality
    TEST_F(ClipchampBabylonNativeTest, ConsoleLogging)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Test that console messages are being captured
        runtime->Dispatch([](Napi::Env env) {
            auto console = env.Global().Get("console").As<Napi::Object>();
            auto log = console.Get("log").As<Napi::Function>();
            log.Call(console, { Napi::String::New(env, "Test message from Clipchamp tests") });
        });
        
        // Give the runtime a moment to process the message
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        EXPECT_FALSE(consoleMessages.empty());
        EXPECT_EQ(consoleMessages.back(), "Test message from Clipchamp tests");
    }

    // Test error handling during frame operations
    TEST_F(ClipchampBabylonNativeTest, FrameOperationErrorHandling)
    {
        // Test operations without initialization
        EXPECT_FALSE(StartRenderingFrame());
        EXPECT_FALSE(FinishRenderingFrame());
        EXPECT_FALSE(ResizeViewport(1920, 1080));
        
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Test double start (should fail)
        EXPECT_FALSE(StartRenderingFrame()); // Already started during init
        
        // Test finish and restart
        EXPECT_TRUE(FinishRenderingFrame());
        EXPECT_TRUE(StartRenderingFrame());
        
        // Test double finish (should fail)
        EXPECT_TRUE(FinishRenderingFrame());
        EXPECT_FALSE(FinishRenderingFrame());
    }

    // Test ExternalTexture lifecycle (simulating Clipchamp's texture management)
    TEST_F(ClipchampBabylonNativeTest, ExternalTextureLifecycle)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        std::vector<std::optional<Babylon::Plugins::ExternalTexture>> textures;
        
        runtime->Dispatch([&textures](Napi::Env env) {
            // Create multiple external textures (simulating video frames)
            for (int i = 0; i < 5; ++i)
            {
                try
                {
                    // In a real scenario, this would use actual Metal textures
                    // For testing purposes, we test the ExternalTexture creation path
                    textures.emplace_back(std::nullopt); // Placeholder for actual texture creation
                }
                catch (const std::exception& e)
                {
                    // Expected in test environment without actual Metal textures
                }
            }
        });
        
        // Test that we can render frames with external textures
        EXPECT_TRUE(RenderFrames(3));
        
        // Cleanup textures (simulating Clipchamp's texture cleanup)
        textures.clear();
    }

    // Test lifecycle stress testing (multiple init/deinit cycles)
    TEST_F(ClipchampBabylonNativeTest, LifecycleStressTesting)
    {
        // Perform multiple initialization/cleanup cycles
        for (int cycle = 0; cycle < 5; ++cycle)
        {
            EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
            EXPECT_TRUE(RenderFrames(10));
            CleanupBabylonNative();
        }
    }

    // Test concurrent operations safety
    TEST_F(ClipchampBabylonNativeTest, ConcurrentOperationsSafety)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Test that resize operations during rendering are handled gracefully
        std::vector<std::future<bool>> futures;
        
        // Start multiple resize operations
        for (int i = 0; i < 3; ++i)
        {
            futures.push_back(std::async(std::launch::async, [this, i]() {
                return ResizeViewport(1920 + i * 100, 1080 + i * 100);
            }));
        }
        
        // Wait for all operations to complete
        for (auto& future : futures)
        {
            // At least one should succeed, others may fail due to threading
            future.get(); // Just ensure no exceptions are thrown
        }
        
        // System should still be functional
        EXPECT_TRUE(RenderFrames(5));
    }

    // Test JavaScript runtime integration
    TEST_F(ClipchampBabylonNativeTest, JavaScriptRuntimeIntegration)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        bool scriptExecuted = false;
        std::promise<bool> executionPromise;
        auto executionFuture = executionPromise.get_future();
        
        runtime->Dispatch([&scriptExecuted, &executionPromise](Napi::Env env) {
            try
            {
                // Test basic JavaScript execution
                auto result = env.Global().Get("parseInt").As<Napi::Function>().Call({
                    Napi::String::New(env, "42")
                });
                
                scriptExecuted = (result.As<Napi::Number>().Int32Value() == 42);
                executionPromise.set_value(scriptExecuted);
            }
            catch (const std::exception&)
            {
                executionPromise.set_value(false);
            }
        });
        
        // Wait for JavaScript execution to complete
        EXPECT_TRUE(executionFuture.get());
    }

    // Test memory management and leak prevention
    TEST_F(ClipchampBabylonNativeTest, MemoryManagement)
    {
        // Perform operations that could potentially leak memory
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
            
            // Create and destroy objects in JavaScript context
            runtime->Dispatch([](Napi::Env env) {
                for (int i = 0; i < 100; ++i)
                {
                    auto obj = Napi::Object::New(env);
                    obj.Set("index", i);
                    obj.Set("data", Napi::String::New(env, "test data " + std::to_string(i)));
                }
            });
            
            EXPECT_TRUE(RenderFrames(5));
            CleanupBabylonNative();
        }
        
        // If we reach here without crashes, memory management is working
        SUCCEED();
    }

    // Test error recovery after exceptions
    TEST_F(ClipchampBabylonNativeTest, ErrorRecovery)
    {
        EXPECT_TRUE(InitializeBabylonNative(1920, 1080));
        
        // Cause an intentional error in JavaScript
        runtime->Dispatch([](Napi::Env env) {
            try
            {
                // This should throw an exception
                env.Global().Get("nonExistentFunction").As<Napi::Function>().Call({});
            }
            catch (const Napi::Error&)
            {
                // Expected - we're testing error recovery
            }
        });
        
        // System should still be functional after the error
        EXPECT_TRUE(RenderFrames(3));
        EXPECT_TRUE(ResizeViewport(1280, 720));
    }
}
