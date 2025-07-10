#include "../Shared/Shared.h"

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>

#include <gtest/gtest.h>

#include <Foundation/NSThread.h>
#include <Metal/Metal.h>

namespace {
    std::optional<Babylon::Graphics::Device> device{};
    std::optional<Babylon::Graphics::DeviceUpdate> deviceUpdate{};
    std::optional<Babylon::AppRuntime> runtime{};

    bool hasStartedRenderingFrame = false;

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

    void InitializeBabylonServices()
    {
        runtime->Dispatch([](Napi::Env env) {
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
}

TEST(Scenario1, Init)
{
    // std::optional<Babylon::Plugins::ExternalTexture> exportTexture{};
    // std::unordered_map<long, Babylon::Plugins::ExternalTexture> sourceTextures;
    // thread_safe_action_queue pendingTextureUpdateQueue;
    // thread_safe_action_queue pendingTextureRemovalQueue;

    // Napi::FunctionReference loadProject;
    // Napi::FunctionReference updateItemTransform;
    // Napi::FunctionReference updateItem;
    // Napi::FunctionReference seek;

    auto mtlDevice = MTLCreateSystemDefaultDevice();

    Babylon::Graphics::Configuration config{};
    config.Device = mtlDevice;
    // config.Window = view; // TODO: Is this needed? If yest then it needs to be an MTKView*.
    // config.Width = static_cast<size_t>(width);
    // config.Height = static_cast<size_t>(height);
    device.emplace(config);

    deviceUpdate.emplace(device->GetUpdate("update"));

    StartRenderingNextFrame();

    runtime.emplace();

    InitializeBabylonServices();
    // [self dispatchBindings];

    // Babylon::ScriptLoader loader{ *runtime };
    // loader.LoadScript("app:///Superfill/superfillCompositor.js");

    // NSString *errorPtr = nil;
    // loader.Dispatch([completion, errorPtr](Napi::Env env) {
    //     seek = Napi::Persistent(env.Global().Get("seek").As<Napi::Function>());
    //     loadProject = Napi::Persistent(env.Global().Get("loadProject").As<Napi::Function>());
    //     updateItemTransform = Napi::Persistent(env.Global().Get("updateTrackItemTransform").As<Napi::Function>());
    //     updateItem = Napi::Persistent(env.Global().Get("updateItem").As<Napi::Function>());
    //     completion(errorPtr);
    // });

    EXPECT_EQ(0, 0);
}
