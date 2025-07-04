#pragma once

#include <memory>

namespace xr
{
    struct VisionOSConfiguration
    {
        // Basic visionOS XR configuration
        bool immersiveMode = false;
    };

    class IXrContextVisionOS
    {
    public:
        virtual ~IXrContextVisionOS() = default;
        
        virtual void SetConfiguration(const VisionOSConfiguration& config) = 0;
        virtual bool IsSessionSupported() const = 0;
        virtual void StartSession() = 0;
        virtual void EndSession() = 0;
        virtual bool IsSessionActive() const = 0;
    };

    std::unique_ptr<IXrContextVisionOS> CreateXrContextVisionOS();
}
