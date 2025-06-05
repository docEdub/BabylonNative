#include "gtest/gtest.h"
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Plugins/ExternalTexture.h>

#if __APPLE__
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#include <AVFoundation/AVFoundation.h>
#endif

namespace ClipchampTests {

/**
 * Mock delegate that implements Clipchamp's BabylonNativeBridgeDelegate interface
 * Used to test the Superfill integration patterns
 */
class MockSuperfillDelegate {
public:
    // Source management callbacks
    std::function<void(long, std::string)> createSourceCallback;
    std::function<void(long, bool, float)> updateConfigCallback;
    std::function<void(long, std::string, double)> updatePlaybackStateCallback;
    std::function<void(long, double)> readFrameCallback;
    std::function<void(long)> destroySourceCallback;
    
    // Audio callbacks
    std::function<void(long, std::string, double)> activateAudioCallback;
    std::function<void(long)> deactivateAudioCallback;
    
    // Project callbacks
    std::function<void(std::string, double)> updateProjectStateCallback;
    std::function<void(std::string)> updateAudioStreamCallback;
    std::function<void(double)> updateProgressCallback;
    
    // Export callbacks
    std::function<void(double)> writeFrameCallback;
    
    // Font and logging callbacks
    std::function<std::string(std::string)> loadFontCallback;
    std::function<void(std::string)> logCallback;
    
    // State tracking
    std::unordered_map<long, std::string> activeSources;
    std::string currentProjectState;
    double currentFrameTime{0.0};
    bool isPlaying{false};
    
    // Mock implementations
    void createSource(long sourceId, const std::string& assetId) {
        activeSources[sourceId] = assetId;
        if (createSourceCallback) {
            createSourceCallback(sourceId, assetId);
        }
    }
    
    void updateConfigForSource(long sourceId, bool loop, float playbackRate) {
        if (updateConfigCallback) {
            updateConfigCallback(sourceId, loop, playbackRate);
        }
    }
    
    void updatePlaybackStateForSource(long sourceId, const std::string& state, double frameTime) {
        currentFrameTime = frameTime;
        if (updatePlaybackStateCallback) {
            updatePlaybackStateCallback(sourceId, state, frameTime);
        }
    }
    
    void readFrameForSource(long sourceId, double frameTime) {
        if (readFrameCallback) {
            readFrameCallback(sourceId, frameTime);
        }
    }
    
    void destroySource(long sourceId) {
        activeSources.erase(sourceId);
        if (destroySourceCallback) {
            destroySourceCallback(sourceId);
        }
    }
    
    void activateSourceAudio(long sourceId, const std::string& streamId, double frameTime) {
        if (activateAudioCallback) {
            activateAudioCallback(sourceId, streamId, frameTime);
        }
    }
    
    void deactivateSourceAudio(long sourceId) {
        if (deactivateAudioCallback) {
            deactivateAudioCallback(sourceId);
        }
    }
    
    void updatePlaybackStateForProject(const std::string& state, double frameTime) {
        currentProjectState = state;
        currentFrameTime = frameTime;
        isPlaying = (state == "playing");
        if (updateProjectStateCallback) {
            updateProjectStateCallback(state, frameTime);
        }
    }
    
    void updateAudioStreamState(const std::string& stateJson) {
        if (updateAudioStreamCallback) {
            updateAudioStreamCallback(stateJson);
        }
    }
    
    void updatePlaybackProgressForProject(double frameTime) {
        currentFrameTime = frameTime;
        if (updateProgressCallback) {
            updateProgressCallback(frameTime);
        }
    }
    
    void writeFrame(double frameTime) {
        if (writeFrameCallback) {
            writeFrameCallback(frameTime);
        }
    }
    
    std::string loadFontData(const std::string& assetId) {
        if (loadFontCallback) {
            return loadFontCallback(assetId);
        }
        return "mock_font_data";
    }
    
    void log(const std::string& message) {
        if (logCallback) {
            logCallback(message);
        }
    }
};

/**
 * Test fixture for Clipchamp's Superfill integration patterns.
 * Based on the Superfill compositor interactions in BabylonNativeBridge.mm
 */
class ClipchampSuperfillIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        delegate = std::make_unique<MockSuperfillDelegate>();
        InitializeForTesting();
    }

    void TearDown() override {
        runtime.reset();
        device.reset();
        delegate.reset();
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
            runtime = std::make_unique<Babylon::AppRuntime>();
            
            // Initialize Babylon services
            runtime->Dispatch([this](Napi::Env env) {
                device->AddToJavaScript(env);
                Babylon::Plugins::NativeEngine::Initialize(env);
                isInitialized = true;
            });
        }
#else
        Babylon::Graphics::Configuration config{};
        config.Width = 1080;
        config.Height = 1920;
        
        device = std::make_unique<Babylon::Graphics::Device>(config);
        runtime = std::make_unique<Babylon::AppRuntime>();
        isInitialized = true;
#endif
    }

    void SimulateProjectLoad(const std::string& projectJson) {
        // Simulate project loading (matches Clipchamp's updateProject)
        currentProject = projectJson;
        
        // Simulate creating sources from project
        if (projectJson.find("video_source") != std::string::npos) {
            delegate->createSource(1, "video_asset_1");
        }
        if (projectJson.find("audio_source") != std::string::npos) {
            delegate->createSource(2, "audio_asset_1");
        }
        
        delegate->updatePlaybackStateForProject("loaded", 0.0);
    }

    void SimulatePlayback() {
        delegate->updatePlaybackStateForProject("playing", 0.0);
        
        // Simulate frame progression
        for (int frame = 0; frame < 10; ++frame) {
            double frameTime = frame * (1.0 / 30.0); // 30 FPS
            delegate->updatePlaybackProgressForProject(frameTime);
            
            // Simulate reading frames for active sources
            for (const auto& source : delegate->activeSources) {
                delegate->readFrameForSource(source.first, frameTime);
            }
        }
    }

    void SimulateAudioActivation() {
        // Activate audio for source 2 (audio source)
        if (delegate->activeSources.count(2)) {
            delegate->activateSourceAudio(2, "stream_1", 0.0);
        }
    }

    void SimulateExport() {
        exportFrameCount = 0;
        
        // Simulate export process
        delegate->updatePlaybackStateForProject("exporting", 0.0);
        
        for (int frame = 0; frame < 30; ++frame) { // 1 second at 30 FPS
            double frameTime = frame * (1.0 / 30.0);
            delegate->writeFrame(frameTime);
            exportFrameCount++;
        }
        
        delegate->updatePlaybackStateForProject("export_complete", 1.0);
    }

    // Test state
    bool isInitialized{false};
    std::string currentProject;
    int exportFrameCount{0};
    std::unique_ptr<MockSuperfillDelegate> delegate;
    
    // BabylonNative components
    std::unique_ptr<Babylon::Graphics::Device> device;
    std::unique_ptr<Babylon::AppRuntime> runtime;
};

// Test basic Superfill project lifecycle
TEST_F(ClipchampSuperfillIntegrationTest, BasicProjectLifecycle) {
    ASSERT_TRUE(isInitialized) << "Should be initialized for Superfill tests";
    
    // Test project loading
    std::string testProject = R"({
        "timeline": {
            "tracks": [
                {"type": "video", "items": [{"id": "video_source", "asset": "video_asset_1"}]},
                {"type": "audio", "items": [{"id": "audio_source", "asset": "audio_asset_1"}]}
            ]
        }
    })";
    
    EXPECT_NO_THROW(SimulateProjectLoad(testProject)) << "Project loading should succeed";
    EXPECT_EQ(delegate->activeSources.size(), 2) << "Should have created 2 sources";
    EXPECT_EQ(delegate->currentProjectState, "loaded") << "Project state should be 'loaded'";
}

// Test source management lifecycle
TEST_F(ClipchampSuperfillIntegrationTest, SourceManagementLifecycle) {
    bool sourceCreated = false;
    bool sourceDestroyed = false;
    long createdSourceId = 0;
    
    // Set up callbacks
    delegate->createSourceCallback = [&](long sourceId, std::string assetId) {
        sourceCreated = true;
        createdSourceId = sourceId;
    };
    
    delegate->destroySourceCallback = [&](long sourceId) {
        sourceDestroyed = true;
        EXPECT_EQ(sourceId, createdSourceId) << "Destroyed source should match created source";
    };
    
    // Create source
    delegate->createSource(100, "test_asset");
    EXPECT_TRUE(sourceCreated) << "Source creation callback should be called";
    EXPECT_EQ(delegate->activeSources.size(), 1) << "Should have one active source";
    
    // Update source configuration
    EXPECT_NO_THROW(delegate->updateConfigForSource(100, true, 1.0f)) 
        << "Source config update should succeed";
    
    // Destroy source
    delegate->destroySource(100);
    EXPECT_TRUE(sourceDestroyed) << "Source destruction callback should be called";
    EXPECT_EQ(delegate->activeSources.size(), 0) << "Should have no active sources";
}

// Test playback state management
TEST_F(ClipchampSuperfillIntegrationTest, PlaybackStateManagement) {
    std::string testProject = R"({"timeline": {"tracks": [{"type": "video", "items": [{"id": "video_source", "asset": "video_asset_1"}]}]}})";
    
    SimulateProjectLoad(testProject);
    ASSERT_EQ(delegate->currentProjectState, "loaded") << "Project should be loaded";
    
    // Test playback states
    delegate->updatePlaybackStateForProject("playing", 0.0);
    EXPECT_EQ(delegate->currentProjectState, "playing") << "Should be in playing state";
    EXPECT_TRUE(delegate->isPlaying) << "isPlaying flag should be true";
    
    delegate->updatePlaybackStateForProject("paused", 1.5);
    EXPECT_EQ(delegate->currentProjectState, "paused") << "Should be in paused state";
    EXPECT_FALSE(delegate->isPlaying) << "isPlaying flag should be false";
    EXPECT_EQ(delegate->currentFrameTime, 1.5) << "Frame time should be updated";
    
    delegate->updatePlaybackStateForProject("stopped", 0.0);
    EXPECT_EQ(delegate->currentProjectState, "stopped") << "Should be in stopped state";
    EXPECT_FALSE(delegate->isPlaying) << "isPlaying flag should be false";
}

// Test frame reading and playback progression
TEST_F(ClipchampSuperfillIntegrationTest, FrameReadingAndPlayback) {
    std::string testProject = R"({"timeline": {"tracks": [{"type": "video", "items": [{"id": "video_source", "asset": "video_asset_1"}]}]}})";
    
    SimulateProjectLoad(testProject);
    
    int frameReadCount = 0;
    double lastFrameTime = 0.0;
    
    delegate->readFrameCallback = [&](long sourceId, double frameTime) {
        frameReadCount++;
        lastFrameTime = frameTime;
    };
    
    delegate->updateProgressCallback = [&](double frameTime) {
        EXPECT_GE(frameTime, lastFrameTime) << "Frame time should be monotonically increasing";
    };
    
    EXPECT_NO_THROW(SimulatePlayback()) << "Playback simulation should succeed";
    EXPECT_GT(frameReadCount, 0) << "Should have read frames during playback";
    EXPECT_GT(lastFrameTime, 0.0) << "Should have progressed in time";
}

// Test audio activation and deactivation
TEST_F(ClipchampSuperfillIntegrationTest, AudioActivationDeactivation) {
    std::string testProject = R"({"timeline": {"tracks": [{"type": "audio", "items": [{"id": "audio_source", "asset": "audio_asset_1"}]}]}})";
    
    SimulateProjectLoad(testProject);
    ASSERT_EQ(delegate->activeSources.size(), 1) << "Should have audio source";
    
    bool audioActivated = false;
    bool audioDeactivated = false;
    
    delegate->activateAudioCallback = [&](long sourceId, std::string streamId, double frameTime) {
        audioActivated = true;
        EXPECT_EQ(sourceId, 2) << "Should activate audio for correct source";
        EXPECT_EQ(streamId, "stream_1") << "Should have correct stream ID";
    };
    
    delegate->deactivateAudioCallback = [&](long sourceId) {
        audioDeactivated = true;
        EXPECT_EQ(sourceId, 2) << "Should deactivate audio for correct source";
    };
    
    // Test audio activation
    EXPECT_NO_THROW(SimulateAudioActivation()) << "Audio activation should succeed";
    EXPECT_TRUE(audioActivated) << "Audio activation callback should be called";
    
    // Test audio deactivation
    delegate->deactivateSourceAudio(2);
    EXPECT_TRUE(audioDeactivated) << "Audio deactivation callback should be called";
}

// Test export workflow
TEST_F(ClipchampSuperfillIntegrationTest, ExportWorkflow) {
    std::string testProject = R"({"timeline": {"tracks": [{"type": "video", "items": [{"id": "video_source", "asset": "video_asset_1"}]}]}})";
    
    SimulateProjectLoad(testProject);
    
    int writeFrameCount = 0;
    std::vector<double> exportFrameTimes;
    
    delegate->writeFrameCallback = [&](double frameTime) {
        writeFrameCount++;
        exportFrameTimes.push_back(frameTime);
    };
    
    EXPECT_NO_THROW(SimulateExport()) << "Export simulation should succeed";
    EXPECT_EQ(writeFrameCount, 30) << "Should have written 30 frames";
    EXPECT_EQ(delegate->currentProjectState, "export_complete") << "Should be in export_complete state";
    
    // Verify frame times are sequential
    for (size_t i = 1; i < exportFrameTimes.size(); ++i) {
        EXPECT_GT(exportFrameTimes[i], exportFrameTimes[i-1]) 
            << "Export frame times should be increasing";
    }
}

// Test font loading integration
TEST_F(ClipchampSuperfillIntegrationTest, FontLoadingIntegration) {
    bool fontLoadCalled = false;
    std::string requestedFontId;
    
    delegate->loadFontCallback = [&](std::string assetId) -> std::string {
        fontLoadCalled = true;
        requestedFontId = assetId;
        return "mock_font_data_for_" + assetId;
    };
    
    std::string fontData = delegate->loadFontData("arial_bold");
    
    EXPECT_TRUE(fontLoadCalled) << "Font load callback should be called";
    EXPECT_EQ(requestedFontId, "arial_bold") << "Should request correct font ID";
    EXPECT_EQ(fontData, "mock_font_data_for_arial_bold") << "Should return expected font data";
}

// Test logging integration
TEST_F(ClipchampSuperfillIntegrationTest, LoggingIntegration) {
    std::vector<std::string> logMessages;
    
    delegate->logCallback = [&](std::string message) {
        logMessages.push_back(message);
    };
    
    // Simulate various log messages
    delegate->log("Superfill: Project loaded");
    delegate->log("Superfill: Playback started");
    delegate->log("Superfill: Frame rendered");
    
    EXPECT_EQ(logMessages.size(), 3) << "Should have captured 3 log messages";
    EXPECT_EQ(logMessages[0], "Superfill: Project loaded") << "First message should match";
    EXPECT_EQ(logMessages[1], "Superfill: Playback started") << "Second message should match";
    EXPECT_EQ(logMessages[2], "Superfill: Frame rendered") << "Third message should match";
}

// Test complex project with multiple tracks and sources
TEST_F(ClipchampSuperfillIntegrationTest, ComplexProjectIntegration) {
    std::string complexProject = R"({
        "timeline": {
            "tracks": [
                {"type": "video", "items": [
                    {"id": "video_1", "asset": "video_asset_1"},
                    {"id": "video_2", "asset": "video_asset_2"}
                ]},
                {"type": "audio", "items": [
                    {"id": "audio_1", "asset": "audio_asset_1"},
                    {"id": "audio_2", "asset": "audio_asset_2"}
                ]},
                {"type": "text", "items": [
                    {"id": "text_1", "text": "Title", "font": "arial_bold"}
                ]}
            ]
        }
    })";
    
    // Track all operations
    std::vector<std::string> operations;
    
    delegate->createSourceCallback = [&](long sourceId, std::string assetId) {
        operations.push_back("create_" + std::to_string(sourceId) + "_" + assetId);
    };
    
    delegate->activateAudioCallback = [&](long sourceId, std::string streamId, double frameTime) {
        operations.push_back("activate_audio_" + std::to_string(sourceId));
    };
    
    delegate->loadFontCallback = [&](std::string assetId) -> std::string {
        operations.push_back("load_font_" + assetId);
        return "font_data";
    };
    
    // Load complex project
    currentProject = complexProject;
    
    // Simulate multiple sources being created
    delegate->createSource(1, "video_asset_1");
    delegate->createSource(2, "video_asset_2");
    delegate->createSource(3, "audio_asset_1");
    delegate->createSource(4, "audio_asset_2");
    
    // Simulate font loading for text track
    delegate->loadFontData("arial_bold");
    
    // Simulate audio activation
    delegate->activateSourceAudio(3, "stream_1", 0.0);
    delegate->activateSourceAudio(4, "stream_2", 0.0);
    
    // Verify all operations occurred
    EXPECT_EQ(delegate->activeSources.size(), 4) << "Should have 4 active sources";
    EXPECT_GE(operations.size(), 6) << "Should have recorded multiple operations";
    
    // Check for expected operations
    bool hasVideoCreation = std::any_of(operations.begin(), operations.end(), 
        [](const std::string& op) { return op.find("create_1_video_asset_1") != std::string::npos; });
    EXPECT_TRUE(hasVideoCreation) << "Should have video source creation";
    
    bool hasAudioActivation = std::any_of(operations.begin(), operations.end(),
        [](const std::string& op) { return op.find("activate_audio") != std::string::npos; });
    EXPECT_TRUE(hasAudioActivation) << "Should have audio activation";
    
    bool hasFontLoading = std::any_of(operations.begin(), operations.end(),
        [](const std::string& op) { return op.find("load_font_arial_bold") != std::string::npos; });
    EXPECT_TRUE(hasFontLoading) << "Should have font loading";
}

} // namespace ClipchampTests
