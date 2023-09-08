#pragma once

#include <Babylon/JsRuntime.h>

namespace Babylon::Polyfills::Internal
{
    class AudioContext : public Napi::ObjectWrap<AudioContext>
    {
        static constexpr auto JS_AUDIOCONTEXT_NAME = "AudioContext";

    public:
        static void Initialize(Napi::Env env);

        AudioContext(const Napi::CallbackInfo& info);

    private:
        JsRuntime& m_runtime;
    };
}
