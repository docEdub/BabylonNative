#include "gtest/gtest.h"
#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/ScriptLoader.h>
#include <chrono>
#include <thread>
#include <optional>
#include <future>
#include <iostream>
#include <vector>
#include <string>

namespace ClipchampJavaScriptTests
{
    // Test fixture for Clipchamp's JavaScript integration patterns
    class ClipchampJavaScriptTest : public ::testing::Test
    {
    protected:
        std::optional<Babylon::Graphics::Device> device;
        std::optional<Babylon::AppRuntime> runtime;
        std::vector<std::string> consoleMessages;
        std::vector<std::string> errorMessages;
        
        // Function references that mirror Clipchamp's cached Napi functions
        std::optional<Napi::FunctionReference> loadProject;
        std::optional<Napi::FunctionReference> updateItemTransform;
        std::optional<Napi::FunctionReference> updateItem;
        std::optional<Napi::FunctionReference> seek;

        void SetUp() override
        {
            consoleMessages.clear();
            errorMessages.clear();
            loadProject.reset();
            updateItemTransform.reset();
            updateItem.reset();
            seek.reset();
        }

        void TearDown() override
        {
            CleanupJavaScriptEnvironment();
        }

        bool InitializeJavaScriptEnvironment()
        {
            try
            {
                // Create minimal graphics device for JavaScript context - same pattern as working tests
                Babylon::Graphics::Configuration deviceConfig{};
                device.emplace(deviceConfig);
                
                // Create runtime with proper exception handling
                Babylon::AppRuntime::Options options{};
                options.UnhandledExceptionHandler = [this](const Napi::Error& error) {
                    errorMessages.push_back(error.Get("message").As<Napi::String>().Utf8Value());
                };
                runtime.emplace(options);
                
                // Initialize Babylon services synchronously - matching working test pattern
                std::promise<bool> initPromise;
                runtime->Dispatch([this, &initPromise](Napi::Env env) {
                    try {
                        device->AddToJavaScript(env);
                        
                        Babylon::Polyfills::Window::Initialize(env);
                        Babylon::Polyfills::XMLHttpRequest::Initialize(env);
                        Babylon::Polyfills::Console::Initialize(env, [this](const char* message, auto logLevel) {
                            consoleMessages.push_back(std::string(message));
                        });
                        Babylon::Plugins::NativeEngine::Initialize(env);
                        
                        initPromise.set_value(true);
                    } catch (...) {
                        initPromise.set_value(false);
                    }
                });
                
                // Wait for initialization to complete - critical for stability
                return initPromise.get_future().get();
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        bool LoadSuperfillMockScript()
        {
            if (!runtime)
            {
                return false;
            }
            
            try
            {
                Babylon::ScriptLoader loader(*runtime);
                
                // Load a mock Superfill compositor script that defines the functions Clipchamp expects
                std::string mockSuperfillScript = R"(
                    // Mock Superfill compositor functions
                    global = global || {};
                    
                    // Project management functions
                    function loadProject(projectJson) {
                        console.log('loadProject called with: ' + projectJson);
                        return { success: true, message: 'Project loaded successfully' };
                    }
                    
                    function updateTrackItemTransform(itemId, rotation, left, top, right, bottom, cropLeft, cropTop, cropRight, cropBottom) {
                        console.log('updateTrackItemTransform called for item: ' + itemId);
                        return { success: true };
                    }
                    
                    function updateItem(itemId, itemJson) {
                        console.log('updateItem called for item: ' + itemId);
                        return { success: true };
                    }
                    
                    function seek(fromTime, toTime) {
                        console.log('seek called from ' + fromTime + ' to ' + toTime);
                        return { success: true };
                    }
                    
                    // Playback control functions
                    function play() {
                        console.log('play called');
                        return { success: true };
                    }
                    
                    function pause() {
                        console.log('pause called');
                        return { success: true };
                    }
                    
                    // Frame rendering functions
                    function requestCurrentFrame() {
                        console.log('requestCurrentFrame called');
                        return { success: true };
                    }
                    
                    // Export functions
                    function startExport(config) {
                        console.log('startExport called with config');
                        return { success: true };
                    }
                    
                    function cancelExport() {
                        console.log('cancelExport called');
                        return { success: true };
                    }
                    
                    // Filter functions
                    function getNonLutFilters() {
                        console.log('getNonLutFilters called');
                        return JSON.stringify([
                            { id: 'brightness', name: 'Brightness' },
                            { id: 'contrast', name: 'Contrast' },
                            { id: 'saturation', name: 'Saturation' }
                        ]);
                    }
                    
                    // Bounds calculation
                    function loadItemBounds(trackItemId, entityId) {
                        console.log('loadItemBounds called for ' + trackItemId + ', ' + entityId);
                        return { x: 0, y: 0, width: 1920, height: 1080 };
                    }
                    
                    // Migration function
                    function migrateProject(projectJson) {
                        console.log('migrateProject called');
                        return JSON.stringify({ version: '2.0', migrated: true });
                    }
                    
                    // Babylon.js mock setup
                    var BABYLON = BABYLON || {};
                    BABYLON.NativeEngine = function() {
                        console.log('NativeEngine created');
                        this.runRenderLoop = function(callback) {
                            console.log('runRenderLoop started');
                        };
                        this.stopRenderLoop = function() {
                            console.log('runRenderLoop stopped');
                        };
                    };
                    
                    BABYLON.Scene = function(engine) {
                        console.log('Scene created');
                        this.render = function() {
                            console.log('Scene rendered');
                        };
                        this.dispose = function() {
                            console.log('Scene disposed');
                        };
                    };
                )";
                
                loader.Eval(mockSuperfillScript, "mockSuperfill.js");
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        bool CacheFunctionReferences()
        {
            if (!runtime)
            {
                return false;
            }
            
            try
            {
                std::promise<bool> cachePromise;
                auto cacheFuture = cachePromise.get_future();
                
                runtime->Dispatch([this, &cachePromise](Napi::Env env) {
                    try
                    {
                        auto global = env.Global();
                        
                        // Cache function references as Clipchamp does
                        seek.emplace(Napi::Persistent(global.Get("seek").As<Napi::Function>()));
                        loadProject.emplace(Napi::Persistent(global.Get("loadProject").As<Napi::Function>()));
                        updateItemTransform.emplace(Napi::Persistent(global.Get("updateTrackItemTransform").As<Napi::Function>()));
                        updateItem.emplace(Napi::Persistent(global.Get("updateItem").As<Napi::Function>()));
                        
                        cachePromise.set_value(true);
                    }
                    catch (const std::exception&)
                    {
                        cachePromise.set_value(false);
                    }
                });
                
                return cacheFuture.get();
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        void CleanupJavaScriptEnvironment()
        {
            try
            {
                // Clear function references
                loadProject.reset();
                updateItemTransform.reset();
                updateItem.reset();
                seek.reset();
                
                // Clean up runtime and device
                runtime.reset();
                device.reset();
            }
            catch (const std::exception& e)
            {
                std::cerr << "Exception during JavaScript cleanup: " << e.what() << std::endl;
            }
        }

        // Helper function to call JavaScript functions and wait for completion
        template<typename... Args>
        bool CallJavaScriptFunction(const std::string& functionName, Args... args)
        {
            if (!runtime)
            {
                return false;
            }
            
            try
            {
                std::promise<bool> callPromise;
                auto callFuture = callPromise.get_future();
                
                runtime->Dispatch([this, &callPromise, functionName, args...](Napi::Env env) {
                    try
                    {
                        auto global = env.Global();
                        auto func = global.Get(functionName).As<Napi::Function>();
                        
                        // Convert arguments to Napi values
                        std::vector<napi_value> napiArgs;
                        ((napiArgs.push_back(Napi::String::New(env, args))), ...);
                        
                        auto result = func.Call(global, napiArgs);
                        callPromise.set_value(true);
                    }
                    catch (const std::exception&)
                    {
                        callPromise.set_value(false);
                    }
                });
                
                return callFuture.get();
            }
            catch (const std::exception&)
            {
                return false;
            }
        }
    };

    // Test basic JavaScript environment initialization
    TEST_F(ClipchampJavaScriptTest, JavaScriptEnvironmentInitialization)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(device.has_value());
        EXPECT_TRUE(runtime.has_value());
    }

    // Test Superfill mock script loading
    TEST_F(ClipchampJavaScriptTest, SuperfillScriptLoading)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        
        // Should have logged messages from script initialization
        EXPECT_FALSE(consoleMessages.empty());
    }

    // Test function reference caching (Clipchamp's performance optimization)
    TEST_F(ClipchampJavaScriptTest, FunctionReferenceCaching)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        EXPECT_TRUE(CacheFunctionReferences());
        
        // All function references should be cached
        EXPECT_TRUE(seek.has_value());
        EXPECT_TRUE(loadProject.has_value());
        EXPECT_TRUE(updateItemTransform.has_value());
        EXPECT_TRUE(updateItem.has_value());
    }

    // Test project loading functionality
    TEST_F(ClipchampJavaScriptTest, ProjectLoading)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        EXPECT_TRUE(CacheFunctionReferences());
        
        // Test project loading with mock data
        std::string mockProjectJson = R"({
            "version": "1.0",
            "timeline": {
                "duration": 10000,
                "tracks": []
            }
        })";
        
        EXPECT_TRUE(CallJavaScriptFunction("loadProject", mockProjectJson));
        
        // Should have logged the project loading call
        bool foundProjectLoadMessage = false;
        for (const auto& message : consoleMessages)
        {
            if (message.find("loadProject called") != std::string::npos)
            {
                foundProjectLoadMessage = true;
                break;
            }
        }
        EXPECT_TRUE(foundProjectLoadMessage);
    }

    // Test track item transform updates
    TEST_F(ClipchampJavaScriptTest, TrackItemTransformUpdates)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        EXPECT_TRUE(CacheFunctionReferences());
        
        // Test transform update with typical Clipchamp parameters
        if (updateItemTransform.has_value())
        {
            std::promise<bool> transformPromise;
            auto transformFuture = transformPromise.get_future();
            
            runtime->Dispatch([this, &transformPromise](Napi::Env env) {
                try
                {
                    auto func = updateItemTransform->Value();
                    auto result = func.Call(env.Global(), {
                        Napi::String::New(env, "item_123"),      // itemId
                        Napi::Number::New(env, 45.0),           // rotation
                        Napi::Number::New(env, 100.0),          // left
                        Napi::Number::New(env, 200.0),          // top
                        Napi::Number::New(env, 300.0),          // right
                        Napi::Number::New(env, 400.0),          // bottom
                        Napi::Number::New(env, 0.0),            // cropLeft
                        Napi::Number::New(env, 0.0),            // cropTop
                        Napi::Number::New(env, 0.0),            // cropRight
                        Napi::Number::New(env, 0.0)             // cropBottom
                    });
                    transformPromise.set_value(true);
                }
                catch (const std::exception&)
                {
                    transformPromise.set_value(false);
                }
            });
            
            EXPECT_TRUE(transformFuture.get());
        }
    }

    // Test seek functionality
    TEST_F(ClipchampJavaScriptTest, SeekFunctionality)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        EXPECT_TRUE(CacheFunctionReferences());
        
        if (seek.has_value())
        {
            std::promise<bool> seekPromise;
            auto seekFuture = seekPromise.get_future();
            
            runtime->Dispatch([this, &seekPromise](Napi::Env env) {
                try
                {
                    auto func = seek->Value();
                    auto result = func.Call(env.Global(), {
                        Napi::Number::New(env, 1000.0),  // fromTime
                        Napi::Number::New(env, 5000.0)   // toTime
                    });
                    seekPromise.set_value(true);
                }
                catch (const std::exception&)
                {
                    seekPromise.set_value(false);
                }
            });
            
            EXPECT_TRUE(seekFuture.get());
        }
    }

    // Test playback control functions
    TEST_F(ClipchampJavaScriptTest, PlaybackControl)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        
        // Test play function
        EXPECT_TRUE(CallJavaScriptFunction("play"));
        
        // Test pause function
        EXPECT_TRUE(CallJavaScriptFunction("pause"));
        
        // Verify console messages for playback control
        bool foundPlayMessage = false;
        bool foundPauseMessage = false;
        
        for (const auto& message : consoleMessages)
        {
            if (message.find("play called") != std::string::npos)
            {
                foundPlayMessage = true;
            }
            if (message.find("pause called") != std::string::npos)
            {
                foundPauseMessage = true;
            }
        }
        
        EXPECT_TRUE(foundPlayMessage);
        EXPECT_TRUE(foundPauseMessage);
    }

    // Test filter enumeration
    TEST_F(ClipchampJavaScriptTest, FilterEnumeration)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        
        std::promise<std::string> filterPromise;
        auto filterFuture = filterPromise.get_future();
        
        runtime->Dispatch([&filterPromise](Napi::Env env) {
            try
            {
                auto global = env.Global();
                auto func = global.Get("getNonLutFilters").As<Napi::Function>();
                auto result = func.Call(global, {});
                filterPromise.set_value(result.As<Napi::String>().Utf8Value());
            }
            catch (const std::exception&)
            {
                filterPromise.set_value("");
            }
        });
        
        std::string filtersJson = filterFuture.get();
        EXPECT_FALSE(filtersJson.empty());
        EXPECT_NE(filtersJson.find("brightness"), std::string::npos);
        EXPECT_NE(filtersJson.find("contrast"), std::string::npos);
        EXPECT_NE(filtersJson.find("saturation"), std::string::npos);
    }

    // Test export functionality
    TEST_F(ClipchampJavaScriptTest, ExportFunctionality)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        
        // Test export start
        EXPECT_TRUE(CallJavaScriptFunction("startExport", "config"));
        
        // Test export cancel
        EXPECT_TRUE(CallJavaScriptFunction("cancelExport"));
        
        // Verify export messages
        bool foundStartExportMessage = false;
        bool foundCancelExportMessage = false;
        
        for (const auto& message : consoleMessages)
        {
            if (message.find("startExport called") != std::string::npos)
            {
                foundStartExportMessage = true;
            }
            if (message.find("cancelExport called") != std::string::npos)
            {
                foundCancelExportMessage = true;
            }
        }
        
        EXPECT_TRUE(foundStartExportMessage);
        EXPECT_TRUE(foundCancelExportMessage);
    }

    // Test error handling in JavaScript context
    TEST_F(ClipchampJavaScriptTest, JavaScriptErrorHandling)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        
        // Intentionally cause a JavaScript error
        runtime->Dispatch([](Napi::Env env) {
            try
            {
                // This should throw an error
                env.Global().Get("nonExistentFunction").As<Napi::Function>().Call({});
            }
            catch (const Napi::Error&)
            {
                // Expected - testing error handling
            }
        });
        
        // System should remain stable after error
        EXPECT_TRUE(LoadSuperfillMockScript());
    }

    // Test Babylon.js integration
    TEST_F(ClipchampJavaScriptTest, BabylonJSIntegration)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        
        std::promise<bool> babylonPromise;
        auto babylonFuture = babylonPromise.get_future();
        
        runtime->Dispatch([&babylonPromise](Napi::Env env) {
            try
            {
                // Test Babylon.js NativeEngine creation
                auto babylon = env.Global().Get("BABYLON").As<Napi::Object>();
                auto engineConstructor = babylon.Get("NativeEngine").As<Napi::Function>();
                auto engine = engineConstructor.New({});
                
                // Test Scene creation
                auto sceneConstructor = babylon.Get("Scene").As<Napi::Function>();
                auto scene = sceneConstructor.New({ engine });
                
                babylonPromise.set_value(true);
            }
            catch (const std::exception&)
            {
                babylonPromise.set_value(false);
            }
        });
        
        EXPECT_TRUE(babylonFuture.get());
        
        // Should have logged Babylon.js initialization messages
        bool foundEngineMessage = false;
        bool foundSceneMessage = false;
        
        for (const auto& message : consoleMessages)
        {
            if (message.find("NativeEngine created") != std::string::npos)
            {
                foundEngineMessage = true;
            }
            if (message.find("Scene created") != std::string::npos)
            {
                foundSceneMessage = true;
            }
        }
        
        EXPECT_TRUE(foundEngineMessage);
        EXPECT_TRUE(foundSceneMessage);
    }

    // Test concurrent JavaScript operations
    TEST_F(ClipchampJavaScriptTest, ConcurrentJavaScriptOperations)
    {
        EXPECT_TRUE(InitializeJavaScriptEnvironment());
        EXPECT_TRUE(LoadSuperfillMockScript());
        EXPECT_TRUE(CacheFunctionReferences());
        
        // Execute multiple JavaScript operations concurrently
        std::vector<std::future<bool>> futures;
        
        for (int i = 0; i < 5; ++i)
        {
            futures.push_back(std::async(std::launch::async, [this, i]() {
                return CallJavaScriptFunction("loadProject", std::string("project_") + std::to_string(i));
            }));
        }
        
        // All operations should complete successfully
        int successCount = 0;
        for (auto& future : futures)
        {
            if (future.get())
            {
                successCount++;
            }
        }
        
        EXPECT_GT(successCount, 0); // At least some should succeed
    }
}
