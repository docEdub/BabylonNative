#include <Babylon/JsRuntimeScheduler.h>

#include <LabSound/LabSound.h>
#include <LabSound/backends/AudioDevice_RtAudio.h>

#include <napi/napi.h>

namespace Babylon::Polyfills::Internal
{
    Napi::Value AudioNodeClass;

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
            assert(m_jsDestinationNode.Value().IsObject());
            return m_jsDestinationNode.Value();
        }

        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        JsRuntimeScheduler m_runtimeScheduler;
        std::shared_ptr<lab::AudioDevice_RtAudio> m_deviceImpl;
        std::shared_ptr<lab::AudioContext> m_impl;
        std::shared_ptr<lab::AudioDestinationNode> m_destinationNodeImpl;

        Napi::ObjectReference m_jsDestinationNode;
    };

    template<class T> class AudioNodeWrap : public Napi::ObjectWrap<T>
    {
        static constexpr auto JS_CLASS_NAME = "AudioNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = Napi::ObjectWrap<T>::DefineClass(
                env,
                JS_CLASS_NAME,
                {
                    // Napi::ObjectWrap<T>::StaticAccessor("prototype", &AudioNodeWrap::GetPrototype, &AudioNodeWrap::SetPrototype),
                    Napi::ObjectWrap<T>::InstanceMethod("connect", &AudioNodeWrap::Connect)
                });

            env.Global().Set(JS_CLASS_NAME, func);

            return func;
        }

        AudioNodeWrap(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<T>(info)
            , m_audioContextImpl{AudioContext::Unwrap(info[0].As<Napi::Object>())->impl()}
        {
        }

        void setImpl(std::shared_ptr<lab::AudioNode> impl)
        {
            m_impl = std::move(impl);
        }

    protected:
        Napi::Value Connect(const Napi::CallbackInfo& info)
        {
            auto jsDestinationNode = info[0].ToObject();
            auto destinationNode = T::Unwrap(jsDestinationNode);
            m_audioContextImpl.connect(destinationNode->m_impl, m_impl);
            return jsDestinationNode;
        }

        template<class ImplT>
        std::shared_ptr<ImplT> impl() const
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

        // static Napi::Value m_jsPrototype;
    };

    class AudioNode : public AudioNodeWrap<AudioNode>
    {
        static constexpr auto JS_CLASS_NAME = "AudioNode";

    public:
        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({ audioContext });
        }

        AudioNode(const Napi::CallbackInfo& info)
            : AudioNodeWrap<AudioNode>(info)
        {
        }
    };

    class GainNode : public AudioNodeWrap<GainNode>
    {
        static constexpr auto JS_CLASS_NAME = "GainNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = ObjectWrap<GainNode>::DefineClass(
                env,
                JS_CLASS_NAME,
                {
                    // Napi::ObjectWrap<GainNode>::StaticAccessor("prototype", &GainNode::GetPrototype, &GainNode::SetPrototype)
                    Napi::ObjectWrap<GainNode>::InstanceMethod("connect", &AudioNodeWrap::Connect)
                });

            env.Global().Set(JS_CLASS_NAME, func);

            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({ audioContext });
        }

        GainNode(const Napi::CallbackInfo& info)
            : AudioNodeWrap<GainNode>{info}
            , m_jsAudioNode{AudioNode::New(info, info[0])}
        {
            setImpl(std::make_shared<lab::GainNode>(m_audioContextImpl));
            return;

            Napi::Function setPrototypeOf = info.Env().Global().Get("Object").ToObject().Get("setPrototypeOf").As<Napi::Function>();
            Napi::Function getPrototypeOf = info.Env().Global().Get("Object").ToObject().Get("getPrototypeOf").As<Napi::Function>();

            //auto audioNodeClass = info.Env().Global().Get("AudioNode");
            auto audioNodeClass = m_jsAudioNode;
            auto audioNodeClassProto = getPrototypeOf.Call({audioNodeClass});

            auto gainNodeClassProto = getPrototypeOf.Call({info.This()});

            setPrototypeOf.Call({gainNodeClassProto, audioNodeClassProto});
            setPrototypeOf.Call({info.This(), m_jsAudioNode});
        }

    private:
        Napi::Object m_jsAudioNode;

        // static Napi::Value GetPrototype(const Napi::CallbackInfo& info)
        // {
        //     return m_jsPrototype;
        // }

        // static void SetPrototype(const Napi::CallbackInfo& info, const Napi::Value& value)
        // {
        //     m_jsPrototype = value;
        // }

        //static Napi::Value m_jsPrototype;
    };

    // Napi::Value GainNode::m_jsPrototype;

    AudioContext::AudioContext(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<AudioContext>{info}
        , m_runtimeScheduler{JsRuntime::GetFromJavaScript(info.Env())}
        , m_deviceImpl{std::make_shared<lab::AudioDevice_RtAudio>(lab::AudioStreamConfig(), GetDefaultAudioDeviceConfiguration())}
        , m_impl{std::make_shared<lab::AudioContext>(false, true)}
        , m_destinationNodeImpl{std::make_shared<lab::AudioDestinationNode>(*m_impl.get(), m_deviceImpl)}
    {
        m_jsDestinationNode = Napi::Persistent(AudioNode::New(info, info.This()));
        assert(m_jsDestinationNode.Value().IsObject());
        auto destinationNode = AudioNode::Unwrap(m_jsDestinationNode.Value());
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

        auto audioNodeClass = Internal::AudioNode::Initialize(env);
        auto gainNodeClass = Internal::GainNode::Initialize(env);

        Napi::Function setPrototypeOf = env.Global().Get("Object").ToObject().Get("setPrototypeOf").As<Napi::Function>();
        auto audioNodeClassPrototype = audioNodeClass.Get("prototype");
        auto gainNodeClassPrototype = gainNodeClass.Get("prototype");

        // Works on Win32 x64 Chakra.
        // Fails on macOS JavaScriptCore -> Uncaught Error: Cannot set prototype of immutable prototype object.
        setPrototypeOf.Call({gainNodeClassPrototype, audioNodeClassPrototype});

        // Works on Win32 x64 Chakra.
        // Works on macOS JavaScriptCore.
        // Fails on Win32 x64 V8 -> Uncaught Error: Cyclic __proto__ value.
        setPrototypeOf.Call({ gainNodeClass, audioNodeClass });
    }
}
