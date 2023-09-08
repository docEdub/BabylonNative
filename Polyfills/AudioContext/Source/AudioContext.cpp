#include "AudioContext.h"

namespace Babylon::Polyfills::Internal
{
    namespace
    {
        constexpr auto JS_CONSTRUCTOR_NAME = "AudioContext";
    }

    void AudioContext::Initialize(Napi::Env env)
    {
        Napi::HandleScope scope{env};

        Napi::Function func = DefineClass(
            env,
            JS_CONSTRUCTOR_NAME,
            {});

        env.Global().Set(JS_CONSTRUCTOR_NAME, func);
    }

    AudioContext::AudioContext(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<AudioContext>{info}
        , m_runtime{JsRuntime::GetFromJavaScript(info.Env())}
    {
    }
}

namespace Babylon::Polyfills::AudioContext
{
    void Initialize(Napi::Env env)
    {
        Internal::AudioContext::Initialize(env);
    }
}
