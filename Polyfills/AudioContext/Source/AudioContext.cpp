#include <Babylon/JsRuntime.h>

#include <napi/napi.h>

namespace Babylon::Polyfills::Internal
{
    class GainNode : public Napi::ObjectWrap<GainNode>
    {
        static constexpr auto JS_CLASS_NAME = "GainNode";

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

        static Napi::Object New(const Napi::CallbackInfo& info)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({});
        }

        GainNode(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<GainNode>{info}
        {
        }
    };

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
                {
                    InstanceMethod("createGain", &AudioContext::CreateGain)
                });

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<AudioContext>{info}
            , m_runtime{JsRuntime::GetFromJavaScript(info.Env())}
        {
        }

    private:
        Napi::Value CreateGain(const Napi::CallbackInfo& info)
        {
            return GainNode::New(info);
        }

        JsRuntime& m_runtime;
    };
}

namespace Babylon::Polyfills::AudioContext
{
    void Initialize(Napi::Env env)
    {
        Internal::AudioContext::Initialize(env);
        Internal::GainNode::Initialize(env);
    }
}
