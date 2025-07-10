#include <Babylon/Graphics/Device.h>

#include <gtest/gtest.h>

#include <Metal/Metal.h>

TEST(Scenario1, Init)
{
    std::optional<Babylon::Graphics::Device> device{};
    std::optional<Babylon::Graphics::DeviceUpdate> deviceUpdate{};
    // std::optional<Babylon::AppRuntime> runtime{};

    // std::optional<Babylon::Plugins::ExternalTexture> exportTexture{};
    // std::unordered_map<long, Babylon::Plugins::ExternalTexture> sourceTextures;
    // thread_safe_action_queue pendingTextureUpdateQueue;
    // thread_safe_action_queue pendingTextureRemovalQueue;

    // Napi::FunctionReference loadProject;
    // Napi::FunctionReference updateItemTransform;
    // Napi::FunctionReference updateItem;
    // Napi::FunctionReference seek;

    bool hasStartedRenderingFrame = false;

    auto mtlDevice = MTLCreateSystemDefaultDevice();

    Babylon::Graphics::Configuration config{};
    config.Device = mtlDevice;
    // config.Window = view; // TODO: Is this needed? If yest then it needs to be an MTKView*.
    // config.Width = static_cast<size_t>(width);
    // config.Height = static_cast<size_t>(height);
    device.emplace(config);

    deviceUpdate.emplace(device->GetUpdate("update"));

    EXPECT_EQ(0, 0);
}
