#pragma once

#if ! __has_feature(objc_arc)
#error "ARC is off"
#endif

#import <CompositorServices/CompositorServices.h>

namespace xr {
    class IXrContextvisionOS {
    public:
        virtual ~IXrContextvisionOS() = default;
        virtual bool IsInitialized() const = 0;
        virtual cp_layer_renderer_t XrLayerRenderer() const = 0;
        virtual cp_frame_t XrFrame() const = 0;
    };
}