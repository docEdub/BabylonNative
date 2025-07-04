#include <Babylon/Graphics/Platform.h>
#include "DeviceImpl.h"

namespace Babylon::Graphics
{
    void DeviceImpl::ConfigureBgfxPlatformData(bgfx::PlatformData& pd, WindowT window)
    {
        pd.nwh = window;
    }

    void DeviceImpl::ConfigureBgfxRenderType(bgfx::PlatformData& /*pd*/, bgfx::RendererType::Enum& /*renderType*/)
    {
    }

    float DeviceImpl::GetDevicePixelRatio(WindowT window)
    {
#if TARGET_OS_VISION
        // On visionOS, the screen property is not available, so return a default scale factor
        (void)window; // Suppress unused parameter warning
        return 2.0f; // Common scale factor for high-resolution displays
#else
        return window.window.screen.backingScaleFactor;
#endif
    }
}
