#include <Babylon/JsRuntime.h>

#include <LabSound/LabSound.h>

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
                {
                    InstanceMethod("createGain", &AudioContext::CreateGain)
                });

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<AudioContext>{info}
            , m_runtime{JsRuntime::GetFromJavaScript(info.Env())}
            , m_impl(std::make_shared<lab::AudioContext>(false))
        {
        }

        lab::AudioContext& impl() const { return *m_impl; }

    private:
        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        JsRuntime& m_runtime;
        std::shared_ptr<lab::AudioContext> m_impl;
    };

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

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({ audioContext });
        }

        GainNode(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<GainNode>{info}
            , m_impl{std::make_shared<lab::GainNode>(AudioContext::Unwrap(info[0].As<Napi::Object>())->impl())}
        {
        }

    private:
        std::shared_ptr<lab::GainNode> m_impl;
    };

    Napi::Value AudioContext::CreateGain(const Napi::CallbackInfo& info)
    {
        return GainNode::New(info, info.This());
    }
}

namespace Babylon::Polyfills::WebAudio
{
    void Initialize(Napi::Env env)
    {
        log_set_quiet(true);
        Internal::AudioContext::Initialize(env);
        Internal::GainNode::Initialize(env);
    }
}
