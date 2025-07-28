#include "../Shared/Shared.h"
#include "../Shared/ThreadSafeActionQueue.h"

#include <Babylon/AppRuntime.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Plugins/ExternalTexture.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>

#include <gtest/gtest.h>

#include <CoreMedia/CMTime.h>

@interface EmptyViewWrapper : NSObject
@property(strong, nonatomic) MTKView* emptyView;
@end

@implementation EmptyViewWrapper
@synthesize emptyView;

- (instancetype)init
{
    // This is a workaround to avoid creating a MTKView instance when running unit tests.
    // This is necessary because the MTKView initializer hangs in XCode 16 and prevents the unit tests from running.
    // This hang doesn't happend when running the actual application, only the test suite.
    if (!NSClassFromString(@"XCTest"))
    {
        emptyView = [[MTKView alloc] init];
    }
    return self;
}
@end

namespace
{
    static auto emptyViewWrapper = [[EmptyViewWrapper alloc] init];
}

class Scenario1Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto mtlDevice = MTLCreateSystemDefaultDevice();

        Babylon::Graphics::Configuration config{};
        config.Device = mtlDevice;
        config.Window = emptyViewWrapper.emptyView;
        config.Width = 1024;
        config.Height = 1024;
        device.emplace(config);

        deviceUpdate.emplace(device->GetUpdate("update"));

        StartRenderingNextFrame();

        Babylon::AppRuntime::Options options{};
        options.UnhandledExceptionHandler = [](const Napi::Error& error) {
            std::cerr << "[Uncaught Error] " << error.Get("stack").As<Napi::String>().Utf8Value() << std::endl;
            std::cerr.flush();
        };
        runtime.emplace(options);

        InitializeBabylonServices();
        DispatchBindings();

        loader.emplace(*runtime);
        loader->LoadScript("app:///Scripts/babylon.max.js");
        loader->LoadScript("app:///Scripts/babylonjs.materials.js");
    }

    void TearDown() override
    {
        std::promise<void> done;
        runtime->Dispatch([&done](Napi::Env env) {
            done.set_value();
        });
        done.get_future().wait();

        Deinitialize();

        std::cerr.flush();
        std::cout.flush();
    }

    void Eval(std::string script, const std::string& name = "code")
    {
        isReady = false;
        loader->Eval(std::move(script), name);
        RunUntilReady();
    }

    void EvalStartupScript()
    {
        Eval(startupScript, "startup");
    }

    void EvalShutdownScript()
    {
        Eval(shutdownScript, "shutdown");
    }

    size_t GetSourceTextureCount() const
    {
        return sourceTextures.size();
    }

private:
    std::string startupScript{R"(
        console.log("Starting up ...");

        var engine = new BABYLON.NativeEngine();
        var scene = new BABYLON.Scene(engine);

        scene.createDefaultCamera(true, true, true);

        engine.runRenderLoop(function () {
            console.log("Rendering frame ...");

            scene.render();

            console.log("Rendering frame - done");
        });

        const shutdown = () => {
            engine.stopRenderLoop();
            scene.dispose();
            engine.dispose();
        };

        console.log("Starting up - done");
        setReady(true);
    )"};

    std::string shutdownScript{R"(
        console.log("Shutting down ...");

        shutdown();

        console.log("Shutting down - done");
        setReady(true);
    )"};

    std::optional<Babylon::Graphics::Device> device{};
    std::optional<Babylon::Graphics::DeviceUpdate> deviceUpdate{};
    std::optional<Babylon::AppRuntime> runtime{};
    std::optional<Babylon::ScriptLoader> loader{};

    bool isExporting = false;
    std::atomic<bool> isReady = false;
    bool hasStartedRenderingFrame = false;

    std::unordered_map<long, Babylon::Plugins::ExternalTexture> sourceTextures;

    thread_safe_action_queue pendingTextureUpdateQueue;
    thread_safe_action_queue pendingTextureRemovalQueue;

    std::optional<Babylon::Plugins::ExternalTexture> exportTexture{};

    void PerformQueuedUpdateActions()
    {
        assert([NSThread isMainThread]);
        pendingTextureUpdateQueue.performQueuedActions();
    }

    void PerformQueuedRemovalActions()
    {
        assert([NSThread isMainThread]);
        pendingTextureRemovalQueue.performQueuedActions();
    }

    void RenderFrame()
    {
        assert([NSThread isMainThread]);
        FinishRenderingCurrentFrame();
        StartRenderingNextFrame();
    }

    void StartRenderingNextFrame()
    {
        assert([NSThread isMainThread]);

        if (!device)
        {
            return;
        }

        if (hasStartedRenderingFrame)
        {
            return;
        }

        hasStartedRenderingFrame = true;
        device->StartRenderingCurrentFrame();
        deviceUpdate->Start();
    }

    void FinishRenderingCurrentFrame()
    {
        assert([NSThread isMainThread]);

        if (!device)
        {
            return;
        }

        if (!hasStartedRenderingFrame)
        {
            return;
        }

        deviceUpdate->Finish();

        PerformQueuedUpdateActions();

        device->FinishRenderingCurrentFrame();

        if (isExporting)
        {
            // Since buffers are queued in order, we can create a new buffer and wait for it complete, which in turn
            // means all previous buffers will also be completed at this point. This is necessary to ensure that the
            // export texture is ready to be read from.
            auto buffer = ((__bridge id<MTLCommandQueue>)device->GetPlatformInfo().CommandQueue).commandBuffer;
            [buffer commit];
            [buffer waitUntilCompleted];
        }

        PerformQueuedRemovalActions();

        hasStartedRenderingFrame = false;
    }

    void InitializeBabylonServices()
    {
        runtime->Dispatch([this](Napi::Env env) {
            device->AddToJavaScript(env);

            auto platformInfo = device->GetPlatformInfo();

            // TODO: Add `createSurfaceTexture` callback?

            Babylon::Polyfills::Window::Initialize(env);
            Babylon::Polyfills::XMLHttpRequest::Initialize(env);
            Babylon::Polyfills::Console::Initialize(env, [](const char* message, Babylon::Polyfills::Console::LogLevel logLevel) {
                std::cout << "[" << EnumToString(logLevel) << "] " << message << std::endl;
                std::cout.flush();
            });
            Babylon::Plugins::NativeEngine::Initialize(env);
        });
    }

    void Deinitialize()
    {
        if (device)
        {
            deviceUpdate->Finish();
            device->FinishRenderingCurrentFrame();
        }

        sourceTextures.clear();

        loader.reset();
        runtime.reset();
        deviceUpdate.reset();
        device.reset();
    }

    void RunUntilReady()
    {
        while (!isReady)
        {
            // Process pending blocks on the main queue.
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]];
        }
    }

    void WriteFrame(CMTime frameTime, std::function<void(bool)> completionHandler)
    {
        assert([NSThread isMainThread]);

        if (!device)
        {
            completionHandler(false);
            return;
        }

        isExporting = true;

        // TODO: Write the frame to a file?
        // See clipchamp-mobile PlayerViewModel.swift:499.
    }

    void StartExporting(id<MTLTexture> texture, std::function<void(bool)> completionHandler)
    {
        assert([NSThread isMainThread]);

        if (!device)
        {
            completionHandler(false);
            return;
        }

        isExporting = true;

        // TODO: Start exporting the texture?
        // In Clipchamp, this is a JavaScript call made via Napi ... `env.Global().Get("startExporting").As<Napi::Function>().Call({ ... });`
        // See clipchamp-mobile mobileApp.ts:228 for the JavaScript implementation of the "startExporting" function.
    }

    /// Creates bindings exposing iOS functionality into javascript code
    void DispatchBindings()
    {
        auto loadTexture = [device = (__bridge id<MTLDevice>)device->GetPlatformInfo().Device]() {
            MTKTextureLoader* loader = [[MTKTextureLoader alloc] initWithDevice:device];
            NSString* filename = @"Checker_albedo_128x128";
            NSString* extension = @"jpg";
            NSURL* url = [[NSBundle mainBundle] URLForResource:filename withExtension:extension];
            NSError* error = nil;
            id<MTLTexture> texture = [loader newTextureWithContentsOfURL:url
                                                                 options:@{MTKTextureLoaderOptionSRGB : @NO,
                                                                 }
                                                                   error:&error];
            if (error != nil)
            {
                NSLog(@"MTKTextureLoader error loading texture %@.%@: %@", filename, extension, error.localizedDescription);
            }

            return Babylon::Plugins::ExternalTexture{texture};
        };

        runtime->Dispatch([this, loadTexture = std::move(loadTexture)](Napi::Env env) {
            // MARK: - Source APIs

            env.Global().Set("createSource", Napi::Function::New(env, [this, loadTexture = std::move(loadTexture)](const Napi::CallbackInfo& info) {
                int sourceId = info[0].As<Napi::Number>().Int32Value();

                Napi::Promise::Deferred deferred{info.Env()};

                // Texture creation on the main thread
                dispatch_async(dispatch_get_main_queue(), ^{
                  auto externalTexture = loadTexture();

                  // Notably we're copying the texture here rather than moving, since we're reusing the original texture in the dispatch below
                  auto insertResult = sourceTextures.insert({sourceId, externalTexture});

                  // Ensure the insert succeeded. This will fail if there was already a value in the map matching the sourceId
                  assert(insertResult.second);

                  runtime->Dispatch([this, deferred, externalTexture = std::move(externalTexture)](Napi::Env env) {
                      // We need to ensure we persist the AddToContextAsync promise
                      auto addToContextPromise = std::make_shared<Napi::Reference<Napi::Promise>>(
                          Napi::Persistent(externalTexture.AddToContextAsync(env)));

                      dispatch_async(dispatch_get_main_queue(), ^{
                        // NB: AddToContextAsync won't resolve until the next frame render, so we need to ensure we render a frame.
                        RenderFrame();
                        runtime->Dispatch([deferred, addToContextPromise = std::move(addToContextPromise)](Napi::Env env) {
                            deferred.Resolve(addToContextPromise->Value());
                        });
                      });
                  });
                });

                return deferred.Promise();
            }));

            env.Global().Set("destroySource", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
                int sourceId = info[0].As<Napi::Number>().Int32Value();

                // The source texture can only be removed between frame renders, so we queue the removal for the next available render.
                pendingTextureRemovalQueue.queueAction([this, sourceId]() {
                    if (sourceTextures.find(sourceId) != sourceTextures.end())
                    {
                        sourceTextures.erase(sourceId);
                    }
                });
            }));

            // MARK: - Export APIs

            env.Global().Set("writeFrame", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
                Napi::Promise::Deferred deferred{info.Env()};
                double timeInMs = info[0].As<Napi::Number>().DoubleValue();
                CMTime frameTime = CMTimeMakeWithSeconds(timeInMs / 1000.0, 300);
                WriteFrame(frameTime, std::function<void(bool)>([this, deferred](bool isFinished) {
                    runtime->Dispatch([this, deferred, isFinished = std::move(isFinished)](Napi::Env env) {
                        deferred.Resolve(Napi::Boolean::New(env, isFinished));

                        if (isFinished)
                        {
                            dispatch_async(dispatch_get_main_queue(), ^{
                              exportTexture.reset();
                            });
                        }
                    });
                }));

                return deferred.Promise();
            }));

            env.Global().Set("renderFrame", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
                Napi::Promise::Deferred deferred{info.Env()};
                dispatch_async(dispatch_get_main_queue(), ^{
                  RenderFrame();
                  runtime->Dispatch([deferred](Napi::Env env) {
                      deferred.Resolve(env.Undefined());
                  });
                });
                return deferred.Promise();
            }));

            env.Global().Set("setReady", Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
                bool ready = info[0].As<Napi::Boolean>().Value();
                this->isReady = ready;
            }));
        });
    }
};

TEST_F(Scenario1Test, StartupAndShutdown)
{
    EvalStartupScript();
    EvalShutdownScript();
}

TEST_F(Scenario1Test, CreateSourceTexture)
{
    EvalStartupScript();

    Eval(R"(
        console.log("Creating source texture ...");

        createSource(0).then((texture) => {
            // TODO: Is there a way to make sure the texture is a valid external texture?
            console.log("Source texture created: " + (texture instanceof BABYLON.ExternalTexture ? "ExternalTexture" : "Unknown type"));
            console.log("typeof texture: " + typeof texture); // prints "typeof texture: object"

            setReady(true);
        });
    )");

    EXPECT_EQ(GetSourceTextureCount(), 1);

    EvalShutdownScript();
}

TEST_F(Scenario1Test, DestroySourceTexture)
{
    EvalStartupScript();

    Eval(R"(
        console.log("Creating source texture ...");

        createSource(0).then((texture) => {
            // TODO: Is there a way to make sure the texture is a valid external texture?
            console.log("Source texture created: " + (texture instanceof BABYLON.ExternalTexture ? "ExternalTexture" : "Unknown type"));
            console.log("typeof texture: " + typeof texture); // prints "typeof texture: object"

            console.log("Creating source texture - done");
            setReady(true);
        });
    )");

    Eval(R"(
        console.log("Destroying source texture ...");

        destroySource(0);

        // Sources are removed between `FinishRenderingCurrentFrame()` and `StartRenderingCurrentFrame`, so we need to
        // render a frame to finalize removing the texture.
        renderFrame().then(() => {
            console.log("Destroying source texture - done");
            setReady(true);
        });
    )");

    EXPECT_EQ(GetSourceTextureCount(), 0);

    EvalShutdownScript();
}

TEST_F(Scenario1Test, WriteFrameToExportTexture)
{
    EvalStartupScript();

    Eval(R"(
        console.log("Creating source texture ...");

        createSource(0).then((texture) => {
            // TODO: Is there a way to make sure the texture is a valid external texture?
            console.log("Source texture created: " + (texture instanceof BABYLON.ExternalTexture ? "ExternalTexture" : "Unknown type"));
            console.log("typeof texture: " + typeof texture); // prints "typeof texture: object"

            console.log("Creating source texture - done");
            setReady(true);
        });
    )");

    EvalShutdownScript();
}
