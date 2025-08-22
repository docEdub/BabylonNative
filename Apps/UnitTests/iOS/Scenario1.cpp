#include "../Shared/Shared.h"

#include <Babylon/Graphics/Device.h>
#include <Babylon/Plugins/ExternalTexture.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Plugins/TestUtils.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/AppRuntime.h>
#include <Babylon/ScriptLoader.h>

#include <arcana/threading/blocking_concurrent_queue.h>
#include <arcana/threading/cancellation.h>

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
    // This hang doesn't happen when running actual applications, only the test suite.
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

    size_t GetSourceTextureCount() const
    {
        return sourceTextures.size();
    }

private:
    std::optional<Babylon::Graphics::Device> device{};
    std::optional<Babylon::Graphics::DeviceUpdate> deviceUpdate{};
    std::optional<Babylon::AppRuntime> runtime{};
    std::optional<Babylon::ScriptLoader> loader{};

    bool isExporting = false;
    std::atomic<bool> isReady = false;
    bool hasStartedRenderingFrame = false;

    std::unordered_map<long, Babylon::Plugins::ExternalTexture> sourceTextures;

    arcana::blocking_concurrent_queue<std::function<void()>> pendingTextureUpdateQueue;
    arcana::blocking_concurrent_queue<std::function<void()>> pendingTextureRemovalQueue;
    arcana::cancellation_source pendingTextureCancel;

    std::optional<Babylon::Plugins::ExternalTexture> exportTexture{};

    void PerformQueuedUpdateActions()
    {
        assert([NSThread isMainThread]);

        std::function<void()> action;
        while (pendingTextureUpdateQueue.try_pop(action, pendingTextureCancel))
        {
            action();
        }
    }

    void PerformQueuedRemovalActions()
    {
        assert([NSThread isMainThread]);

        std::function<void()> action;
        while (pendingTextureRemovalQueue.try_pop(action, pendingTextureCancel))
        {
            action();
        }
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
            Babylon::Plugins::TestUtils::Initialize(env, emptyViewWrapper.emptyView);
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
            RenderFrame();
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

        // TODO: Compare the exported frame to an expected result.
        // Use same image comparison code as playground validation/visualization tests.
        // See clipchamp-mobile PlayerViewModel.swift:499.
        completionHandler(true);
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
            // For available options see:
            //  - https://developer.apple.com/documentation/metalkit/mtktextureloader/option?language=objc
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
                pendingTextureRemovalQueue.push([this, sourceId]() {
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

                dispatch_async(dispatch_get_main_queue(), ^{
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
                });

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

namespace
{
    const std::string StartupScript{R"(
        console.log("Starting up ...");

        var engine = new BABYLON.NativeEngine();
        var scene = new BABYLON.Scene(engine);

        scene.createDefaultCamera(true, true, true);

        engine.runRenderLoop(function () {
            // console.log("Rendering frame ...");

            scene.render();

            // console.log("Rendering frame - done");
        });

        const shutdown = () => {
            engine.stopRenderLoop();
            scene.dispose();
            engine.dispose();
        };

        console.log("Starting up - done");
        setReady(true);
    )"};

    const std::string ShutdownScript{R"(
        console.log("Shutting down ...");

        shutdown();

        console.log("Shutting down - done");
        setReady(true);
    )"};

    const std::string CreateTestFunctionsScript{R"(
        console.log("Creating test functions ...");

        async function GetReferenceImageData() {
            console.log("Getting reference image data ...");

            // return new Promise((resolve, reject) => {
            //     console.log("Getting reference image data - done");
            //     resolve([]);
            // });

            return new Promise((resolve, reject) => {
                // TODO: Get this to load from "app:///ReferenceImages/Scenario1_reference.png"
                // - might just need to do full CMake reconfigure.
                const url = "app:///Scenario1_reference.png";

                const onLoadFileError = function (request, exception) {
                    console.error("Failed to retrieve " + url + ".", exception);
                    reject(exception);
                };

                const onload = function (data, responseURL) {
                    if (typeof (data) === "string") {
                        throw new Error("Decode Image from string data not yet implemented.");
                    }

                    const referenceImage = TestUtils.decodeImage(data);
                    const referenceImageData = TestUtils.getImageData(referenceImage);

                    console.log("Getting reference image data - done");

                    resolve(referenceImageData);
                };

                BABYLON.Tools.LoadFile(url, onload, undefined, undefined, /*useArrayBuffer*/true, onLoadFileError);
            });
        }

        function CompareImages(imageDataA, imageDataB) {
            console.log("Comparing images ...");

            if (!imageDataA) {
                console.log("No rendered image data available.");
                return;
            }

            if (!imageDataB) {
                console.log("No reference image data available.");
                return;
            }

            if (imageDataA.length != imageDataB.length) {
                throw new Error(`Reference data length (${imageDataB.length}) must match render data length (${imageDataA.length})`);
            }

            const size = imageDataA.length;
            let differencesCount = 0;

            const threshold = 25; // Threshold for pixel color difference

            for (let index = 0; index < size; index += 4) {
                if (Math.abs(imageDataA[index] - imageDataB[index]) < threshold &&
                    Math.abs(imageDataA[index + 1] - imageDataB[index + 1]) < threshold &&
                    Math.abs(imageDataA[index + 2] - imageDataB[index + 2]) < threshold) {
                    continue;
                }

                if (differencesCount === 0) {
                    console.log(`First pixel off at ${index}: Value: (${imageDataA[index]}, ${imageDataA[index + 1]}, ${imageDataA[index] + 2}) - Expected: (${imageDataB[index]}, ${imageDataB[index + 1]}, ${imageDataB[index + 2]}) `);
                }

                imageDataB[index] = 255;
                imageDataB[index + 1] *= 0.5;
                imageDataB[index + 2] *= 0.5;
                differencesCount++;
            }

            if (differencesCount) {
                console.log("Pixel difference: " + differencesCount + " pixels.");
            } else {
                console.log("No pixel difference!");
            }

            const errorRatio = (differencesCount * 100) / (size / 4);
            console.log(`Error ratio = ${errorRatio}`);

            console.log("Comparing images - done");
        }

        setReady(true);

        console.log("Creating test functions - done");
    )"};

    const std::string CreateSourceScript{R"(
        console.log("Creating source texture ...");

        let sourceTexture = null;

        createSource(0).then((texture) => {
            // TODO: Is there a way to make sure the texture is a valid external texture?
            console.log("Source texture created: " + (texture instanceof BABYLON.ExternalTexture ? "ExternalTexture" : "Unknown type"));
            console.log("typeof texture: " + typeof texture); // prints "typeof texture: object"

            console.log("Wrapping texture ...");
            const wrappedTexture = engine.wrapNativeTexture(texture);
            console.log("Wrapping texture - done");

            // Object.getOwnPropertyNames(wrappedTexture).forEach((prop) => {
            //     console.log(`   wrappedTexture.${prop}`);
            // });

            console.log("Creating source texture ...");
            // sourceTexture = new BABYLON.ThinTexture(wrappedTexture);
            sourceTexture = new BABYLON.Texture(null, scene, {
                internalTexture: wrappedTexture,
                onError: (message, exception) => {
                    console.error("Error creating source texture:", message, exception);
                },
                onLoad: () => {
                    console.log("Source texture loaded successfully.");
                },
            });
            console.log("Creating source texture - done");

            setReady(true);
        });
    )"};
}

// TEST_F(Scenario1Test, StartupAndShutdown)
// {
//     Eval(StartupScript, "StartupScript");
//     Eval(ShutdownScript, "ShutdownScript");
// }

// TEST_F(Scenario1Test, CreateSourceTexture)
// {
//     Eval(StartupScript, "StartupScript");
//     Eval(CreateSourceScript, "CreateSourceScript");

//     EXPECT_EQ(GetSourceTextureCount(), 1);

//     Eval(ShutdownScript, "ShutdownScript");
// }

TEST_F(Scenario1Test, DestroySourceTexture)
{
    Eval(StartupScript, "StartupScript");
    Eval(CreateTestFunctionsScript, "CreateTestFunctionsScript");
    Eval(CreateSourceScript, "CreateSourceScript");

    Eval(R"(
        console.log("Saving render as image ...");

        let ok = true;

        if (engine) {
            console.log("Engine is initialized.");
        } else {
            console.log("Engine not initialized.");
            ok = false;
        }

        if (ok && engine._engine) {
            console.log("Native engine is initialized.");
        } else {
            console.log("Native engine not initialized.");
            ok = false;
        }

        if (ok && sourceTexture) {
            console.log("Source texture is initialized.");
        } else {
            console.log("Source texture not initialized.");
            ok = false;
        }

        if (ok) {
            const plane = BABYLON.MeshBuilder.CreatePlane("plane", { size: 2 }, scene);
            const planeMaterial = new BABYLON.StandardMaterial("planeMaterial", scene);
            planeMaterial.emissiveTexture = sourceTexture;

            console.log("Setting plane material ...");
            plane.material = planeMaterial;
            console.log("Setting plane material - done");

            plane.position.z = 1;

            console.log("scene.executeWhenReady ...");
            scene.executeWhenReady(() => {
                console.log("Getting engine frame buffer data ...");

                engine._engine.getFrameBufferData((frameBufferData) => {
                    // Make a copy to retain the data correctly after `GetReferenceImageData` resolves.
                    const renderedImageData = frameBufferData.slice();

                    GetReferenceImageData().then((referenceImageData) => {
                        CompareImages(renderedImageData, referenceImageData);

                        const outputDirectory = "/Users/andy/-/code/BabylonNative/Results/";
                        TestUtils.writePNG(referenceImageData, 1024, 1024, outputDirectory + "reference.png");
                        TestUtils.writePNG(renderedImageData, 1024, 1024, outputDirectory + "rendered.png");

                        setReady(true);
                    });
                });
            });
            console.log("scene.executeWhenReady - done");
        }
    )");

    // Eval(R"(
    //     console.log("Destroying source texture ...");

    //     destroySource(0);

    //     // Sources are removed between `FinishRenderingCurrentFrame()` and `StartRenderingCurrentFrame`, so we need to
    //     // render a frame to finalize removing the texture.
    //     renderFrame().then(() => {
    //         console.log("Destroying source texture - done");
    //         setReady(true);
    //     });
    // )");

    // EXPECT_EQ(GetSourceTextureCount(), 0);

    Eval(ShutdownScript, "ShutdownScript");
}

// TEST_F(Scenario1Test, WriteFrameToExportTexture)
// {
//     Eval(StartupScript, "StartupScript");
//     Eval(CreateSourceScript, "CreateSourceScript");

//     Eval(R"(
//         console.log("Writing frame ...");

//         writeFrame(0).then((isFinished) => {
//             console.log("Writing frame - done: `isFinished` = " + isFinished);
//             setReady(true);
//         });
//     )");

//     Eval(ShutdownScript, "ShutdownScript");
// }
