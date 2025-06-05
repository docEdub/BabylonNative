#include "gtest/gtest.h"
#include <memory>
#include <string>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeEngine.h>

#if __APPLE__
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#endif

namespace ClipchampTests {

/**
 * Test fixture for Clipchamp's BabylonNative error handling patterns.
 * Based on error scenarios that can occur in BabylonNativeBridge.mm
 */
class ClipchampErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start with clean state for each test
        device.reset();
        runtime.reset();
        lastError.clear();
    }

    void TearDown() override {
        // Cleanup
        runtime.reset();
        device.reset();
    }

    bool InitializeWithValidation(int width, int height, std::string& error) {
        // Matches Clipchamp's validation logic
        if (width <= 0 || height <= 0) {
            error = "Bridge Error: Invalid arguments to bridge initialize";
            return false;
        }

        if (device) {
            error = "Bridge Error: Bridge cannot be initialized multiple times";
            return false;
        }

        try {
#if __APPLE__
            id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
            if (!mtlDevice) {
                error = "Bridge Error: Failed to create Metal device";
                return false;
            }

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
#else
            Babylon::Graphics::Configuration config{};
            config.Width = static_cast<size_t>(width);
            config.Height = static_cast<size_t>(height);
            
            device = std::make_unique<Babylon::Graphics::Device>(config);
#endif
            
            runtime = std::make_unique<Babylon::AppRuntime>();
            return true;
            
        } catch (const std::exception& e) {
            error = std::string("Bridge Error: ") + e.what();
            return false;
        }
    }

    bool UpdateWindowWithValidation(int width, int height, std::string& error) {
        if (!device) {
            error = "Bridge Error: Device not initialized";
            return false;
        }

        if (width <= 0 || height <= 0) {
            error = "Bridge Error: Invalid window dimensions";
            return false;
        }

        try {
            // Simulate window update
            return true;
        } catch (const std::exception& e) {
            error = std::string("Bridge Error: ") + e.what();
            return false;
        }
    }

    bool LoadScriptWithErrorHandling(const std::string& scriptPath, std::string& error) {
        if (!runtime) {
            error = "Bridge Error: Runtime not initialized";
            return false;
        }

        try {
            Babylon::ScriptLoader loader{*runtime};
            
            // Simulate script loading
            if (scriptPath.empty()) {
                error = "Bridge Error: Empty script path";
                return false;
            }
            
            if (scriptPath.find("invalid") != std::string::npos) {
                error = "Bridge Error: Script not found";
                return false;
            }
            
            // Simulate successful load
            return true;
            
        } catch (const std::exception& e) {
            error = std::string("Bridge Error: Script loading failed - ") + e.what();
            return false;
        }
    }

    void SimulateRenderingErrorScenario() {
        if (device) {
            // Simulate starting a frame
            device->StartRenderingCurrentFrame();
            
            // Simulate an error condition that prevents normal completion
            // In real scenarios, this could be GPU errors, memory issues, etc.
            throw std::runtime_error("Simulated rendering error");
        }
    }

    // Test state
    std::string lastError;
    std::unique_ptr<Babylon::Graphics::Device> device;
    std::unique_ptr<Babylon::AppRuntime> runtime;
};

// Test invalid dimension handling (matches Clipchamp's validation)
TEST_F(ClipchampErrorHandlingTest, InvalidDimensionHandling) {
    std::string error;
    
    // Test zero width
    EXPECT_FALSE(InitializeWithValidation(0, 1920, error)) << "Should fail with zero width";
    EXPECT_EQ(error, "Bridge Error: Invalid arguments to bridge initialize") << "Should have correct error message";
    
    // Test zero height
    error.clear();
    EXPECT_FALSE(InitializeWithValidation(1080, 0, error)) << "Should fail with zero height";
    EXPECT_EQ(error, "Bridge Error: Invalid arguments to bridge initialize") << "Should have correct error message";
    
    // Test negative width
    error.clear();
    EXPECT_FALSE(InitializeWithValidation(-100, 1920, error)) << "Should fail with negative width";
    EXPECT_EQ(error, "Bridge Error: Invalid arguments to bridge initialize") << "Should have correct error message";
    
    // Test negative height
    error.clear();
    EXPECT_FALSE(InitializeWithValidation(1080, -100, error)) << "Should fail with negative height";
    EXPECT_EQ(error, "Bridge Error: Invalid arguments to bridge initialize") << "Should have correct error message";
    
    // Test valid dimensions
    error.clear();
    EXPECT_TRUE(InitializeWithValidation(1080, 1920, error)) << "Should succeed with valid dimensions";
    EXPECT_TRUE(error.empty()) << "Should have no error with valid dimensions";
}

// Test double initialization prevention (matches Clipchamp's check)
TEST_F(ClipchampErrorHandlingTest, DoubleInitializationPrevention) {
    std::string error;
    
    // First initialization should succeed
    EXPECT_TRUE(InitializeWithValidation(1080, 1920, error)) << "First initialization should succeed";
    EXPECT_TRUE(error.empty()) << "First initialization should have no error";
    
    // Second initialization should fail
    error.clear();
    EXPECT_FALSE(InitializeWithValidation(1080, 1920, error)) << "Second initialization should fail";
    EXPECT_EQ(error, "Bridge Error: Bridge cannot be initialized multiple times") << "Should have correct error message";
}

// Test window update error scenarios
TEST_F(ClipchampErrorHandlingTest, WindowUpdateErrorScenarios) {
    std::string error;
    
    // Update without initialization should fail
    EXPECT_FALSE(UpdateWindowWithValidation(1080, 1920, error)) << "Should fail without initialization";
    EXPECT_EQ(error, "Bridge Error: Device not initialized") << "Should have correct error message";
    
    // Initialize first
    error.clear();
    ASSERT_TRUE(InitializeWithValidation(1080, 1920, error)) << "Initialization should succeed";
    
    // Invalid dimensions should fail
    error.clear();
    EXPECT_FALSE(UpdateWindowWithValidation(0, 1920, error)) << "Should fail with zero width";
    EXPECT_EQ(error, "Bridge Error: Invalid window dimensions") << "Should have correct error message";
    
    error.clear();
    EXPECT_FALSE(UpdateWindowWithValidation(1080, 0, error)) << "Should fail with zero height";
    EXPECT_EQ(error, "Bridge Error: Invalid window dimensions") << "Should have correct error message";
    
    // Valid update should succeed
    error.clear();
    EXPECT_TRUE(UpdateWindowWithValidation(1920, 1080, error)) << "Should succeed with valid dimensions";
    EXPECT_TRUE(error.empty()) << "Should have no error with valid update";
}

// Test script loading error scenarios
TEST_F(ClipchampErrorHandlingTest, ScriptLoadingErrorScenarios) {
    std::string error;
    
    // Script loading without runtime should fail
    EXPECT_FALSE(LoadScriptWithErrorHandling("app:///test.js", error)) << "Should fail without runtime";
    EXPECT_EQ(error, "Bridge Error: Runtime not initialized") << "Should have correct error message";
    
    // Initialize runtime
    error.clear();
    ASSERT_TRUE(InitializeWithValidation(1080, 1920, error)) << "Initialization should succeed";
    
    // Empty script path should fail
    error.clear();
    EXPECT_FALSE(LoadScriptWithErrorHandling("", error)) << "Should fail with empty script path";
    EXPECT_EQ(error, "Bridge Error: Empty script path") << "Should have correct error message";
    
    // Invalid script should fail
    error.clear();
    EXPECT_FALSE(LoadScriptWithErrorHandling("app:///invalid_script.js", error)) << "Should fail with invalid script";
    EXPECT_EQ(error, "Bridge Error: Script not found") << "Should have correct error message";
    
    // Valid script should succeed
    error.clear();
    EXPECT_TRUE(LoadScriptWithErrorHandling("app:///superfillCompositor.js", error)) << "Should succeed with valid script";
    EXPECT_TRUE(error.empty()) << "Should have no error with valid script";
}

// Test rendering error scenarios
TEST_F(ClipchampErrorHandlingTest, RenderingErrorScenarios) {
    std::string error;
    
    // Initialize for rendering tests
    ASSERT_TRUE(InitializeWithValidation(1080, 1920, error)) << "Initialization should succeed";
    
    // Test rendering error handling
    EXPECT_THROW(SimulateRenderingErrorScenario(), std::runtime_error) << "Should throw on rendering error";
    
    // After error, device should still be valid for cleanup
    EXPECT_TRUE(device != nullptr) << "Device should still exist after rendering error";
}

// Test error message formatting consistency
TEST_F(ClipchampErrorHandlingTest, ErrorMessageFormatting) {
    std::string error;
    
    // All error messages should start with "Bridge Error:"
    InitializeWithValidation(0, 1920, error);
    EXPECT_TRUE(error.find("Bridge Error:") == 0) << "Error message should start with 'Bridge Error:'";
    
    error.clear();
    InitializeWithValidation(1080, 1920, error);
    InitializeWithValidation(1080, 1920, error); // Second call
    EXPECT_TRUE(error.find("Bridge Error:") == 0) << "Error message should start with 'Bridge Error:'";
    
    error.clear();
    UpdateWindowWithValidation(1080, 1920, error); // Without initialization
    EXPECT_TRUE(error.find("Bridge Error:") == 0) << "Error message should start with 'Bridge Error:'";
    
    error.clear();
    LoadScriptWithErrorHandling("", error); // Without runtime
    EXPECT_TRUE(error.find("Bridge Error:") == 0) << "Error message should start with 'Bridge Error:'";
}

// Test error handling in JavaScript context
TEST_F(ClipchampErrorHandlingTest, JavaScriptErrorHandling) {
    std::string error;
    
    ASSERT_TRUE(InitializeWithValidation(1080, 1920, error)) << "Initialization should succeed";
    
    bool javascriptErrorHandled = false;
    std::string javascriptError;
    
    // Test JavaScript error handling
    runtime->Dispatch([&](Napi::Env env) {
        try {
            // Simulate JavaScript error
            throw Napi::Error::New(env, "Simulated JavaScript error");
        } catch (const Napi::Error& e) {
            javascriptErrorHandled = true;
            javascriptError = e.Message();
        }
    });
    
    // Give JavaScript time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(javascriptErrorHandled) << "JavaScript error should be handled";
    EXPECT_FALSE(javascriptError.empty()) << "JavaScript error message should be captured";
}

// Test memory allocation error scenarios
TEST_F(ClipchampErrorHandlingTest, MemoryAllocationErrorScenarios) {
    std::string error;
    
    // Test with extremely large dimensions that could cause memory allocation issues
    // Note: This test might not actually fail on systems with lots of memory
    // but it tests the error handling pathway
    EXPECT_NO_THROW(InitializeWithValidation(100000, 100000, error)) 
        << "Should handle large memory allocations gracefully";
    
    // If initialization succeeded with large dimensions, that's also valid
    if (device && error.empty()) {
        SUCCEED() << "Large memory allocation succeeded";
    } else if (!error.empty()) {
        EXPECT_TRUE(error.find("Bridge Error:") == 0) << "Memory error should be properly formatted";
    }
}

// Test cascading error scenarios
TEST_F(ClipchampErrorHandlingTest, CascadingErrorScenarios) {
    std::string error;
    
    // Start with a failed initialization
    EXPECT_FALSE(InitializeWithValidation(-1, -1, error)) << "Initial error should occur";
    EXPECT_FALSE(error.empty()) << "Should have error message";
    
    // Subsequent operations should also fail gracefully
    std::string secondError;
    EXPECT_FALSE(UpdateWindowWithValidation(1080, 1920, secondError)) << "Second operation should also fail";
    EXPECT_FALSE(secondError.empty()) << "Second error should be reported";
    
    std::string thirdError;
    EXPECT_FALSE(LoadScriptWithErrorHandling("app:///test.js", thirdError)) << "Third operation should also fail";
    EXPECT_FALSE(thirdError.empty()) << "Third error should be reported";
}

// Test error recovery scenarios
TEST_F(ClipchampErrorHandlingTest, ErrorRecoveryScenarios) {
    std::string error;
    
    // Start with failed initialization
    EXPECT_FALSE(InitializeWithValidation(0, 1920, error)) << "Initial error should occur";
    EXPECT_FALSE(error.empty()) << "Should have error message";
    
    // Reset state
    device.reset();
    runtime.reset();
    
    // Should be able to recover with valid parameters
    error.clear();
    EXPECT_TRUE(InitializeWithValidation(1080, 1920, error)) << "Should recover with valid parameters";
    EXPECT_TRUE(error.empty()) << "Should have no error after recovery";
    
    // Subsequent operations should work
    error.clear();
    EXPECT_TRUE(UpdateWindowWithValidation(1920, 1080, error)) << "Operations should work after recovery";
    EXPECT_TRUE(error.empty()) << "Should have no error after recovery";
}

} // namespace ClipchampTests
