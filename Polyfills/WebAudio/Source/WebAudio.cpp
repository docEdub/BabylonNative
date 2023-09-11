#include <Babylon/JsRuntimeScheduler.h>

#include <LabSound/LabSound.h>
#include <LabSound/backends/AudioDevice_RtAudio.h>

#include <napi/napi.h>

namespace Babylon::Polyfills::Internal
{
    lab::AudioStreamConfig GetDefaultAudioDeviceConfiguration()
    {
        const std::vector<lab::AudioDeviceInfo> audioDevices = lab::AudioDevice_RtAudio::MakeAudioDeviceList();

        lab::AudioDeviceInfo defaultOutputInfo;
        for (auto& info : audioDevices) {
            if (info.is_default_output)
                defaultOutputInfo = info;
        }

        lab::AudioStreamConfig outputConfig;
        if (defaultOutputInfo.index != -1) {
            outputConfig.device_index = defaultOutputInfo.index;
            outputConfig.desired_channels = std::min(uint32_t(2), defaultOutputInfo.num_output_channels);
            outputConfig.desired_samplerate = defaultOutputInfo.nominal_samplerate;
        }

        return outputConfig;
    }

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
                    InstanceAccessor("destination", &AudioContext::GetDestination, nullptr),
                    InstanceMethod("createGain", &AudioContext::CreateGain)
                });

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info);

        lab::AudioContext& impl() const { return *m_impl; }

    private:
        Napi::Value GetDestination(const Napi::CallbackInfo& info)
        {
            return m_jsDestinationNode;
        }

        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        JsRuntimeScheduler m_runtimeScheduler;
        std::shared_ptr<lab::AudioDevice_RtAudio> m_deviceImpl;
        std::shared_ptr<lab::AudioContext> m_impl;
        std::shared_ptr<lab::AudioDestinationNode> m_destinationNodeImpl;

        Napi::Object m_jsDestinationNode;
    };

    class AudioNode : public Napi::ObjectWrap<AudioNode>
    {
    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::HandleScope scope{env};

            Napi::Function func = DefineClass(
                env,
                "AudioNode",
                {
                    InstanceMethod("connect", &AudioNode::Connect)
                });

            env.Global().Set("AudioNode", func);

            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get("AudioNode").As<Napi::Function>().New({audioContext});
        }

        AudioNode(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<AudioNode>(info)
            , m_audioContextImpl{AudioContext::Unwrap(info[0].As<Napi::Object>())->impl()}
        {
        }

        void setImpl(std::shared_ptr<lab::AudioNode> impl)
        {
            m_impl = std::move(impl);
        }

    protected:
        template<class ImplT> std::shared_ptr<ImplT> impl() const
        {
            return std::reinterpret_pointer_cast<ImplT>(m_impl);
        }

        lab::AudioContext& m_audioContextImpl;
        std::shared_ptr<lab::AudioNode> m_impl;

    private:
        // static Napi::Value GetPrototype(const Napi::CallbackInfo& info)
        // {
        //     return m_jsPrototype;
        // }

        // static void SetPrototype(const Napi::CallbackInfo& info, const Napi::Value& value)
        // {
        //     m_jsPrototype = value;
        // }

        Napi::Value Connect(const Napi::CallbackInfo& info)
        {
            auto jsDestinationNode = info[0].ToObject();
            auto destinationNode = AudioNode::Unwrap(jsDestinationNode);
            //m_audioContextImpl.connect(destinationNode->m_impl, m_impl);
            return jsDestinationNode;
        }

        // static Napi::Value m_jsPrototype;
    };

    //class AudioNode : public AudioNodeWrap<AudioNode>
    //{
    //public:
    //    static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
    //    {
    //        return info.Env().Global().Get("AudioNode").As<Napi::Function>().New({audioContext});
    //    }

    //    AudioNode(const Napi::CallbackInfo& info)
    //        : AudioNodeWrap<AudioNode>(info)
    //    {
    //    }
    //};

    class GainNode : public Napi::ObjectWrap<GainNode>
    {
    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::HandleScope scope{env};

            Napi::Function func = DefineClass(
                env,
                "GainNode",
                {
                    // Napi::ObjectWrap<GainNode>::StaticAccessor("prototype", &GainNode::GetPrototype, &GainNode::SetPrototype)
                });

            env.Global().Set("GainNode", func);

            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get("GainNode").As<Napi::Function>().New({audioContext});
        }

        GainNode(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<GainNode>{info}
        {
            //setImpl(std::make_shared<lab::GainNode>(m_audioContextImpl));
        }

    private:
        // static Napi::Value GetPrototype(const Napi::CallbackInfo& info)
        // {
        //     return m_jsPrototype;
        // }

        // static void SetPrototype(const Napi::CallbackInfo& info, const Napi::Value& value)
        // {
        //     m_jsPrototype = value;
        // }

        static Napi::Value m_jsPrototype;
    };

    // Napi::Value GainNode::m_jsPrototype;

    AudioContext::AudioContext(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<AudioContext>{info}
        , m_runtimeScheduler{JsRuntime::GetFromJavaScript(info.Env())}
        , m_deviceImpl{std::make_shared<lab::AudioDevice_RtAudio>(lab::AudioStreamConfig(), GetDefaultAudioDeviceConfiguration())}
        , m_impl{std::make_shared<lab::AudioContext>(false, true)}
        , m_destinationNodeImpl{std::make_shared<lab::AudioDestinationNode>(*m_impl.get(), m_deviceImpl)}
    {
        m_jsDestinationNode = AudioNode::New(info, info.This());
        auto destinationNode = AudioNode::Unwrap(m_jsDestinationNode);
        destinationNode->setImpl(m_destinationNodeImpl);

        m_deviceImpl->setDestinationNode(m_destinationNodeImpl);
        m_impl->setDestinationNode(m_destinationNodeImpl);
    }

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

        Napi::Function getPrototypeOf = env.Global().Get("Object").ToObject().Get("getPrototypeOf").As<Napi::Function>();
        Napi::Function setPrototypeOf = env.Global().Get("Object").ToObject().Get("setPrototypeOf").As<Napi::Function>();


        /* 1
        auto audioNodeClass = Internal::AudioNode::Initialize(env);
        auto gainNodeClass = Internal::GainNode::Initialize(env);
        auto audioNodeClassPrototype = audioNodeClass.Get("prototype");
        auto gainNodeClassPrototype = gainNodeClass.Get("prototype");

        // Works on Win32 x64 Chakra.
        // Fails on Win32 x64 V8 -> Uncaught Error: Object.setPrototypeOf called on null or undefined.
        // Fails on macOS JavaScriptCore -> Uncaught Error: Cannot set prototype of immutable prototype object.
        setPrototypeOf.Call({ gainNodeClassPrototype, audioNodeClassPrototype });

        // Works on Win32 x64 Chakra.
        setPrototypeOf.Call({ gainNodeClass, audioNodeClass });
        */

        /* 2
        auto audioNodeClass = Internal::AudioNode::Initialize(env).ToObject().Get("constructor").ToObject();
        auto gainNodeClass = Internal::GainNode::Initialize(env).ToObject().Get("constructor").ToObject();
        auto audioNodeClassPrototype = audioNodeClass.Get("prototype");
        auto gainNodeClassPrototype = gainNodeClass.Get("prototype");

        // Fails on Win32 x64 V8 -> Uncaught Error: Cyclic __proto__ value.
        setPrototypeOf.Call({gainNodeClassPrototype, audioNodeClassPrototype});

        setPrototypeOf.Call({gainNodeClass, audioNodeClass});
        */

        /* 3 */
        auto objectClass = env.Global().Get("Object").ToObject().Get("constructor").ToObject();
        auto audioNodeClass = Internal::AudioNode::Initialize(env);
        auto gainNodeClass = Internal::GainNode::Initialize(env);
        //Internal::AudioNode::Initialize(env);
        //Internal::GainNode::Initialize(env);
        auto audioNodeClass2 = env.Global().Get("AudioNode").ToObject();
        auto gainNodeClass2 = env.Global().Get("GainNode").ToObject();
        //auto objectClassPrototype = objectClass.Get("prototype").ToObject();
        //auto audioNodeClassPrototype = audioNodeClass.Get("prototype");//.ToObject();
        //auto gainNodeClassPrototype = gainNodeClass.Get("prototype");//.ToObject();
        auto objectClassPrototype = getPrototypeOf.Call({objectClass}).ToObject();
        auto audioNodeClassPrototype = getPrototypeOf.Call({audioNodeClass2}).ToObject();
        auto gainNodeClassPrototype = getPrototypeOf.Call({gainNodeClass2}).ToObject();

        //setPrototypeOf.Call({audioNodeClassPrototype, objectClassPrototype});
        //setPrototypeOf.Call({audioNodeClass, objectClass});

        //setPrototypeOf.Call({gainNodeClassPrototype, objectClassPrototype});
        //setPrototypeOf.Call({gainNodeClass, objectClass});

        gainNodeClassPrototype.Set("_af_proto_", "gainNodeClassPrototype");
        auto gainProtoId = gainNodeClassPrototype.Get("_af_proto_").ToString().Utf8Value();
        auto audioNodeProtoId = audioNodeClassPrototype.Get("_af_proto_").ToString().Utf8Value();

        auto objectProtoId = objectClassPrototype.Get("_af_proto_").ToString().Utf8Value();

        gainNodeClass2.Set("_af_class_", "gainNodeClass");
        auto gainId = gainNodeClass2.Get("_af_class_").ToString().Utf8Value();
        auto audioNodeId = audioNodeClass2.Get("_af_class_").ToString().Utf8Value();

        auto isAudioClassUndefined = audioNodeClass.IsUndefined();
        auto isGainNodeClassUndefined = gainNodeClass.IsUndefined();
        auto isAudioNodeProtoUndefined = audioNodeClassPrototype.IsUndefined();
        auto isGainNodeProtoUndefined = gainNodeClassPrototype.IsUndefined();

        setPrototypeOf.Call({gainNodeClassPrototype, audioNodeClassPrototype});
        setPrototypeOf.Call({gainNodeClass, audioNodeClass});
    }
}
