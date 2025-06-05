#include "gtest/gtest.h"
#include <memory>
#include <chrono>
#include <thread>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Plugins/NativeEngine.h>

#if __APPLE__
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#endif

namespace ClipchampTests {

/**
 * Test fixture for Clipchamp's BabylonNative rendering lifecycle patterns.
 * Based on the rendering loop used in BabylonNativeBridge.mm
 */
class ClipchampRenderingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the basic components needed for rendering
        InitializeForTesting();
        hasStartedRenderingFrame = false;
        frameCount = 0;
        lastFrameTime = std::chrono::high_resolution_clock::now();
    }

    void TearDown() override {
        // Ensure we finish any pending frame before cleanup
        if (hasStartedRenderingFrame) {
            FinishRenderingCurrentFrame();
        }
        
        runtime.reset();
        deviceUpdate.reset();
        device.reset();
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
        }
#else
        Babylon::Graphics::Configuration config{};
        config.Width = 1080;
        config.Height = 1920;
        
        device = std::make_unique<Babylon::Graphics::Device>(config);
        deviceUpdate = std::make_unique<Babylon::Graphics::DeviceUpdate>(device->GetUpdate("update"));
        runtime = std::make_unique<Babylon::AppRuntime>();
#endif
    }

    // Rendering lifecycle methods that match Clipchamp's implementation
    void StartRenderingNextFrame() {
        if (hasStartedRenderingFrame) {
            // Clipchamp prevents starting a new frame when one is already in progress
            throw std::runtime_error("Cannot start rendering frame when one is already in progress");
        }
        
        if (device && deviceUpdate) {
            device->StartRenderingCurrentFrame();
            hasStartedRenderingFrame = true;
            frameCount++;
            lastFrameTime = std::chrono::high_resolution_clock::now();
        }
    }

    void FinishRenderingCurrentFrame() {
        if (!hasStartedRenderingFrame) {
            // Clipchamp prevents finishing a frame that hasn't been started
            throw std::runtime_error("Cannot finish rendering frame when none is in progress");
        }
        
        if (device && deviceUpdate) {
            device->FinishRenderingCurrentFrame();
            deviceUpdate->Finish();
            hasStartedRenderingFrame = false;
        }
    }

    void SimulateRenderLoop(int frameCount) {
        for (int i = 0; i < frameCount; ++i) {
            StartRenderingNextFrame();
            
            // Simulate some rendering work
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
            
            FinishRenderingCurrentFrame();
        }
    }

    double CalculateMeanFps() {
        if (frameCount == 0) return 0.0;
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - testStartTime);
        
        if (duration.count() == 0) return 0.0;
        
        return (frameCount * 1000.0) / duration.count();
    }

    // Test state tracking
    bool hasStartedRenderingFrame;
    int frameCount;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    std::chrono::high_resolution_clock::time_point testStartTime{std::chrono::high_resolution_clock::now()};

    // BabylonNative components
    std::unique_ptr<Babylon::Graphics::Device> device;
    std::unique_ptr<Babylon::Graphics::DeviceUpdate> deviceUpdate;
    std::unique_ptr<Babylon::AppRuntime> runtime;
};

// Test basic frame rendering lifecycle
TEST_F(ClipchampRenderingTest, BasicFrameRenderingLifecycle) {
    // Should be able to start a frame
    EXPECT_NO_THROW(StartRenderingNextFrame()) << "Should be able to start rendering a frame";
    EXPECT_TRUE(hasStartedRenderingFrame) << "Frame rendering should be marked as started";
    
    // Should be able to finish the frame
    EXPECT_NO_THROW(FinishRenderingCurrentFrame()) << "Should be able to finish rendering a frame";
    EXPECT_FALSE(hasStartedRenderingFrame) << "Frame rendering should be marked as finished";
}

// Test prevention of double frame start (matches Clipchamp's hasStartedRenderingFrame check)
TEST_F(ClipchampRenderingTest, PreventDoubleFrameStart) {
    // Start first frame
    EXPECT_NO_THROW(StartRenderingNextFrame()) << "First frame start should succeed";
    
    // Attempting to start another frame should fail
    EXPECT_THROW(StartRenderingNextFrame(), std::runtime_error) 
        << "Should prevent starting a second frame before finishing the first";
    
    // Should still be able to finish the original frame
    EXPECT_NO_THROW(FinishRenderingCurrentFrame()) << "Should be able to finish the original frame";
}

// Test prevention of finishing frame that wasn't started
TEST_F(ClipchampRenderingTest, PreventFinishingUnstartedFrame) {
    // Attempting to finish without starting should fail
    EXPECT_THROW(FinishRenderingCurrentFrame(), std::runtime_error)
        << "Should prevent finishing a frame that wasn't started";
}

// Test multiple frame rendering sequence
TEST_F(ClipchampRenderingTest, MultipleFrameSequence) {
    const int frameCount = 10;
    
    for (int i = 0; i < frameCount; ++i) {
        EXPECT_NO_THROW(StartRenderingNextFrame()) 
            << "Frame " << i << " start should succeed";
        EXPECT_NO_THROW(FinishRenderingCurrentFrame()) 
            << "Frame " << i << " finish should succeed";
    }
    
    EXPECT_EQ(this->frameCount, frameCount) << "Should have rendered the expected number of frames";
}

// Test render loop simulation (matches Clipchamp's typical usage pattern)
TEST_F(ClipchampRenderingTest, RenderLoopSimulation) {
    const int testFrameCount = 30; // Simulate 30 frames (~0.5 seconds at 60 FPS)
    
    EXPECT_NO_THROW(SimulateRenderLoop(testFrameCount)) 
        << "Render loop simulation should complete without errors";
    
    EXPECT_EQ(frameCount, testFrameCount) 
        << "Should have rendered the expected number of frames";
    
    EXPECT_FALSE(hasStartedRenderingFrame) 
        << "Should not have any pending frame after render loop";
}

// Test FPS calculation (matches Clipchamp's calculateMeanEditorPreviewFps)
TEST_F(ClipchampRenderingTest, FpsCalculation) {
    // Initial FPS should be 0
    EXPECT_EQ(CalculateMeanFps(), 0.0) << "Initial FPS should be 0";
    
    // Simulate some frames
    const int testFrameCount = 60;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SimulateRenderLoop(testFrameCount);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double expectedFps = (testFrameCount * 1000.0) / duration.count();
    double actualFps = CalculateMeanFps();
    
    // Allow for some variance due to timing precision
    EXPECT_NEAR(actualFps, expectedFps, 5.0) << "FPS calculation should be reasonably accurate";
    EXPECT_GT(actualFps, 0.0) << "FPS should be greater than 0 after rendering frames";
}

// Test rendering with different frame intervals
TEST_F(ClipchampRenderingTest, VariableFrameIntervals) {
    // Test different frame intervals to simulate variable rendering loads
    const std::vector<int> frameIntervals = {10, 20, 30, 16}; // Different millisecond intervals
    
    for (int interval : frameIntervals) {
        StartRenderingNextFrame();
        
        // Simulate variable processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        
        EXPECT_NO_THROW(FinishRenderingCurrentFrame()) 
            << "Should handle variable frame intervals correctly";
    }
}

// Test frame rendering error recovery
TEST_F(ClipchampRenderingTest, FrameRenderingErrorRecovery) {
    // Start a frame
    EXPECT_NO_THROW(StartRenderingNextFrame()) << "Frame start should succeed";
    
    // Simulate an error condition by manually resetting the flag
    hasStartedRenderingFrame = false;
    
    // Attempting to finish should now fail
    EXPECT_THROW(FinishRenderingCurrentFrame(), std::runtime_error)
        << "Should detect inconsistent frame state";
    
    // Reset to clean state
    hasStartedRenderingFrame = false;
    
    // Should be able to start fresh
    EXPECT_NO_THROW(StartRenderingNextFrame()) << "Should be able to recover and start new frame";
    EXPECT_NO_THROW(FinishRenderingCurrentFrame()) << "Should be able to finish recovered frame";
}

// Test concurrent frame operations safety
TEST_F(ClipchampRenderingTest, ConcurrentFrameOperationsSafety) {
    // This test ensures that the frame state tracking is thread-safe
    // In actual usage, Clipchamp calls these from the main thread, but let's verify safety
    
    bool threadStarted = false;
    std::exception_ptr threadException;
    
    std::thread renderThread([this, &threadStarted, &threadException]() {
        try {
            threadStarted = true;
            
            // Attempt to start frame from another thread
            StartRenderingNextFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            FinishRenderingCurrentFrame();
        } catch (...) {
            threadException = std::current_exception();
        }
    });
    
    // Wait for thread to start
    while (!threadStarted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Main thread should be blocked from starting another frame
    EXPECT_THROW(StartRenderingNextFrame(), std::runtime_error)
        << "Should prevent concurrent frame operations";
    
    renderThread.join();
    
    // Check if thread had any exceptions
    if (threadException) {
        std::rethrow_exception(threadException);
    }
}

} // namespace ClipchampTests
