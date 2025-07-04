#pragma once

#include <CompositorServices/CompositorServices.h>

typedef struct IXrContextVisionOS
{
    virtual bool IsInitialized() const = 0;
    virtual bool IsImmersiveSessionActive() const = 0;
    virtual void SetImmersiveSessionActive(bool active) = 0;
    virtual void* GetCompositorLayer() const = 0;
    virtual void SetCompositorLayer(void* layer) = 0;
} IXrContextVisionOS;
