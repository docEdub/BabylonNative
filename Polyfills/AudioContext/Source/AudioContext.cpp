#include <Babylon/JsRuntime.h>

#include <napi/napi.h>

namespace Babylon::Polyfills::Internal
{
    class AudioContext : public Napi::ObjectWrap<AudioContext>
    {
        static constexpr auto JS_CLASS_NAME = "AudioContext";

    public:
        static void Initialize(Napi::Env env)
        {
            Napi::HandleScope scope{env};

            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                {});

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<AudioContext>{info}
            , m_runtime{JsRuntime::GetFromJavaScript(info.Env())}
        {
        }

    private:
        JsRuntime& m_runtime;
    };
}

namespace Babylon::Polyfills::AudioContext
{
    void Initialize(Napi::Env env)
    {
        Internal::AudioContext::Initialize(env);
    }
}
