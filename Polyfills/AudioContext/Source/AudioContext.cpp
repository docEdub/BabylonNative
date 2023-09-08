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

        Napi::Function constructor = DefineClass(
            env,
            JS_CONSTRUCTOR_NAME,
            {});

        JsRuntime::NativeObject::GetFromJavaScript(env).Set(JS_CONSTRUCTOR_NAME, constructor);
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
