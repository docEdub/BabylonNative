#include <LabSound/LabSound.h>
#include <LabSound/backends/AudioDevice_Miniaudio.h>

#include <memory>
#include <napi/napi.h>

namespace Babylon::Polyfills::Internal
{
    lab::AudioStreamConfig GetDefaultAudioDeviceConfiguration()
    {
        const std::vector<lab::AudioDeviceInfo> audioDevices = lab::AudioDevice_Miniaudio::MakeAudioDeviceList();

        lab::AudioDeviceInfo defaultOutputInfo;
        for (auto& info : audioDevices)
        {
            if (info.is_default_output)
            {
                defaultOutputInfo = info;
            }
        }

        lab::AudioStreamConfig outputConfig;
        if (defaultOutputInfo.index != -1)
        {
            outputConfig.device_index = defaultOutputInfo.index;
            outputConfig.desired_channels = std::min(uint32_t(2), defaultOutputInfo.num_output_channels);
            outputConfig.desired_samplerate = defaultOutputInfo.nominal_samplerate;
        }

        return outputConfig;
    }

    class AudioContext final : public Napi::ObjectWrap<AudioContext>
    {
        static constexpr auto JS_CLASS_NAME = "AudioContext";

    public:
        static void Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                {
                    StaticMethod(Napi::Symbol::WellKnown(env, "hasInstance"), &AudioContext::HasInstance),
                    InstanceAccessor("destination", &AudioContext::GetDestination, nullptr),
                    InstanceMethod("createGain", &AudioContext::CreateGain)
                });

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info);

        lab::AudioContext& Impl() const { return *m_impl; }

    private:
        static Napi::Value HasInstance(const Napi::CallbackInfo& info)
        {
            auto getPrototypeOf = info.Env().Global().Get("Object").ToObject().Get("getPrototypeOf").As<Napi::Function>();
            auto prototype = getPrototypeOf.Call({info[0]});
            while (prototype != info.Env().Null())
            {
                if (prototype == getPrototypeOf.Call({info.Env().Global().Get(JS_CLASS_NAME)})
                        || prototype == info.Env().Global().Get(JS_CLASS_NAME).ToObject().Get("prototype"))
                {
                    return Napi::Value::From(info.Env(), true);
                }
                prototype = getPrototypeOf.Call({prototype});
            }
            return Napi::Value::From(info.Env(), false);
        }

        Napi::Value GetDestination(const Napi::CallbackInfo& info)
        {
            assert(m_jsDestinationNode.Value().IsObject());
            return m_jsDestinationNode.Value();
        }

        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        std::shared_ptr<lab::AudioDevice_Miniaudio> m_deviceImpl;
        std::shared_ptr<lab::AudioContext> m_impl;
        std::shared_ptr<lab::AudioDestinationNode> m_destinationNodeImpl;

        Napi::ObjectReference m_jsDestinationNode;
    };

    class AudioParam final : public Napi::ObjectWrap<AudioParam>
    {
        static constexpr auto JS_CLASS_NAME = "AudioParam";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                {
                    InstanceAccessor("value", &AudioParam::GetValue, &AudioParam::SetValue),
                    InstanceMethod("setTargetAtTime", &AudioParam::SetTargetAtTime)
                });
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, std::shared_ptr<lab::AudioParam> impl)
        {
            auto jsAudioParam = info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({});
            auto audioParam = AudioParam::Unwrap(jsAudioParam);
            audioParam->SetImpl(impl);
            return jsAudioParam;
        }

        AudioParam(const Napi::CallbackInfo& info)
            : ObjectWrap<AudioParam>(info)
        {
        }

        void SetImpl(std::shared_ptr<lab::AudioParam> impl)
        {
            m_impl = std::move(impl);
        }

    private:
        Napi::Value GetValue(const Napi::CallbackInfo& info)
        {
            return Napi::Value::From(info.Env(), m_impl->value());
        }

        void SetValue(const Napi::CallbackInfo& info, const Napi::Value& value)
        {
            m_impl->setValue(value.As<Napi::Number>().FloatValue());
        }

        void SetTargetAtTime(const Napi::CallbackInfo& info)
        {
            if (info.Length() < 3)
            {
                throw Napi::TypeError::New(info.Env(), std::string(
                    "Failed to execute 'setTargetAtTime' on 'AudioParam': 3 arguments required, but only " + std::to_string(info.Length()) + " present."
                    ));
            }

            auto jsTarget = info[0];
            auto jsStartTime = info[1];
            auto jsTimeConstant = info[2];
            if (!jsTarget.IsNumber() || !jsStartTime.IsNumber() || !jsTimeConstant.IsNumber())
            {
                throw Napi::TypeError::New(info.Env(),
                    "Failed to execute 'setTargetAtTime' on 'AudioParam': The provided float value is non-finite."
                    );
            }

            auto target = jsTarget.As<Napi::Number>().FloatValue();
            auto startTime = jsStartTime.As<Napi::Number>().FloatValue();
            auto timeConstant = jsTimeConstant.As<Napi::Number>().FloatValue();

            m_impl->setTargetAtTime(target, startTime, timeConstant);
        }

        std::shared_ptr<lab::AudioParam> m_impl;
    };

    class AudioNodeBase
    {
    public:
        AudioNodeBase(const Napi::CallbackInfo& info)
            : m_audioContextImpl{AudioContext::Unwrap(info[0].As<Napi::Object>())->Impl()}
        {
        }

        Napi::Value Connect(const Napi::CallbackInfo& info);

    protected:
        lab::AudioContext& AudioContextImpl() const
        {
            return m_audioContextImpl;
        }

        template<class ImplT>
        std::shared_ptr<ImplT> Impl() const
        {
            return std::static_pointer_cast<ImplT>(m_impl);
        }

        void SetImpl(std::shared_ptr<lab::AudioNode> impl)
        {
            m_impl = std::move(impl);
        }

        lab::AudioContext& m_audioContextImpl;
        std::shared_ptr<lab::AudioNode> m_impl;
    };

    class AudioScheduledSourceNodeBase : public AudioNodeBase
    {
    public:
        AudioScheduledSourceNodeBase(const Napi::CallbackInfo& info)
            : AudioNodeBase(info)
        {
        }

        Napi::Value Start(const Napi::CallbackInfo& info)
        {
            float when = 0.f;
            if (0 < info.Length())
            {
                when = info[0].ToNumber().FloatValue();
            }

            Impl()->start(when);

            return info.Env().Undefined();
        }

        Napi::Value Stop(const Napi::CallbackInfo& info)
        {
            float when = 0.f;
            if (0 < info.Length())
            {
                when = info[0].ToNumber().FloatValue();
            }

            Impl()->stop(when);

            return info.Env().Undefined();
        }

    private:
        std::shared_ptr<lab::AudioScheduledSourceNode> Impl() const
        {
            return AudioNodeBase::Impl<lab::AudioScheduledSourceNode>();
        }
    };

    template<class T>
    class AudioNodeWrap : public Napi::ObjectWrap<T>, public AudioNodeBase
    {
    public:
        template<typename... Args>
        static const std::initializer_list<Napi::ClassPropertyDescriptor<T>>& Properties(Args&&... args)
        {
            static std::initializer_list<Napi::ClassPropertyDescriptor<T>> properties = {
                Napi::ObjectWrap<T>::InstanceMethod("connect", &AudioNodeBase::Connect),
                args...};
            return properties;
        }

        AudioNodeWrap(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<T>(info)
            , AudioNodeBase(info)
        {
        }
    };

    template<class T>
    class AudioScheduledSourceNodeWrap : public Napi::ObjectWrap<T>, public AudioScheduledSourceNodeBase
    {
    public:
        AudioScheduledSourceNodeWrap(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<T>(info)
            , AudioScheduledSourceNodeBase(info)
        {
        }

    protected:
        template<typename... Args>
        static const std::initializer_list<Napi::ClassPropertyDescriptor<T>>& Properties(Args&&... args)
        {
            return AudioNodeWrap<T>::Properties(
                Napi::ObjectWrap<T>::InstanceMethod("start", &AudioScheduledSourceNodeWrap::Start),
                Napi::ObjectWrap<T>::InstanceMethod("stop", &AudioScheduledSourceNodeWrap::Stop),
                args...);
        }
    };

    class AudioNode final : public AudioNodeWrap<AudioNode>
    {
        static constexpr auto JS_CLASS_NAME = "AudioNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                Properties(
                    StaticMethod(Napi::Symbol::WellKnown(env, "hasInstance"), &AudioNode::HasInstance)
                ));
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext, std::shared_ptr<lab::AudioNode> impl)
        {
            auto jsAudioNode = info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({audioContext});
            auto audioNode = AudioNode::Unwrap(jsAudioNode);
            audioNode->SetImpl(impl);
            return jsAudioNode;
        }

        AudioNode(const Napi::CallbackInfo& info)
            : AudioNodeWrap<AudioNode>(info)
        {
        }

    private:
        static Napi::Value HasInstance(const Napi::CallbackInfo& info)
        {
            auto constructor = info[0];
            if (!constructor.IsFunction())
            {
                constructor = info[0].ToObject().Get("constructor");
            }

            auto getPrototypeOf = info.Env().Global().Get("Object").ToObject().Get("getPrototypeOf").As<Napi::Function>();
            auto prototype = getPrototypeOf.Call({constructor});
            while (prototype != info.Env().Null())
            {
                if (prototype == getPrototypeOf.Call({info.Env().Global().Get(JS_CLASS_NAME)})
                        || prototype == info.Env().Global().Get(JS_CLASS_NAME).ToObject().Get("prototype"))
                {
                    return Napi::Value::From(info.Env(), true);
                }
                prototype = getPrototypeOf.Call({prototype});
            }

            return Napi::Value::From(info.Env(), false);
        }
    };

    class AudioScheduledSourceNode : public AudioScheduledSourceNodeWrap<AudioScheduledSourceNode>
    {
        static constexpr auto JS_CLASS_NAME = "AudioScheduledSourceNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(env, JS_CLASS_NAME, Properties());
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        AudioScheduledSourceNode(const Napi::CallbackInfo& info)
            : AudioScheduledSourceNodeWrap<AudioScheduledSourceNode>(info)
        {
        }
    };

    class GainNode final : public AudioNodeWrap<GainNode>
    {
        static constexpr auto JS_CLASS_NAME = "GainNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                Properties(
                    StaticMethod(Napi::Symbol::WellKnown(env, "hasInstance"), &GainNode::HasInstance),
                    InstanceAccessor("gain", &GainNode::GetGain, nullptr)
                ));
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({audioContext});
        }

        GainNode(const Napi::CallbackInfo& info)
            : AudioNodeWrap<GainNode>{info}
        {
            SetImpl(std::make_shared<lab::GainNode>(m_audioContextImpl));
            m_jsGain = Napi::Persistent(AudioParam::New(info, Impl()->gain()));
        }

    private:
        static Napi::Value HasInstance(const Napi::CallbackInfo& info)
        {
            auto getPrototypeOf = info.Env().Global().Get("Object").ToObject().Get("getPrototypeOf").As<Napi::Function>();
            auto prototype = getPrototypeOf.Call({info[0]});
            while (prototype != info.Env().Null())
            {
                if (prototype == getPrototypeOf.Call({info.Env().Global().Get(JS_CLASS_NAME)})
                        || prototype == info.Env().Global().Get(JS_CLASS_NAME).ToObject().Get("prototype"))
                {
                    return Napi::Value::From(info.Env(), true);
                }
                prototype = getPrototypeOf.Call({prototype});
            }
            return Napi::Value::From(info.Env(), false);
        }

        Napi::Value GetGain(const Napi::CallbackInfo& info)
        {
            return m_jsGain.Value();
        }

        std::shared_ptr<lab::GainNode> Impl() const
        {
            return AudioNodeBase::Impl<lab::GainNode>();
        }

        Napi::ObjectReference m_jsGain;
    };

    class OscillatorNode final : public AudioScheduledSourceNodeWrap<OscillatorNode>
    {
        static constexpr auto JS_CLASS_NAME = "OscillatorNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                Properties(
                    InstanceAccessor("frequency", &OscillatorNode::GetFrequency, nullptr)
                ));
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({audioContext});
        }

        OscillatorNode(const Napi::CallbackInfo& info)
            : AudioScheduledSourceNodeWrap<OscillatorNode>{info}
        {
            SetImpl(std::make_shared<lab::OscillatorNode>(m_audioContextImpl));
            m_jsFrequency = Napi::Persistent(AudioParam::New(info, Impl()->frequency()));
        }

    private:
        Napi::Value GetFrequency(const Napi::CallbackInfo& info)
        {
            return m_jsFrequency.Value();
        }

        std::shared_ptr<lab::OscillatorNode> Impl() const
        {
            return AudioNodeBase::Impl<lab::OscillatorNode>();
        }

        Napi::ObjectReference m_jsFrequency;
    };

    AudioContext::AudioContext(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<AudioContext>{info}
        , m_deviceImpl{std::make_shared<lab::AudioDevice_Miniaudio>(lab::AudioStreamConfig(), GetDefaultAudioDeviceConfiguration())}
        , m_impl{std::make_shared<lab::AudioContext>(false, true)}
        , m_destinationNodeImpl{std::make_shared<lab::AudioDestinationNode>(*m_impl.get(), m_deviceImpl)}
    {
        m_jsDestinationNode = Napi::Persistent(AudioNode::New(info, info.This(), m_destinationNodeImpl));
        m_deviceImpl->setDestinationNode(m_destinationNodeImpl);
        m_impl->setDestinationNode(m_destinationNodeImpl);
    }

    Napi::Value AudioContext::CreateGain(const Napi::CallbackInfo& info)
    {
        return GainNode::New(info, info.This());
    }

    Napi::Value AudioNodeBase::Connect(const Napi::CallbackInfo& info)
    {
        auto jsDestinationNode = info[0].ToObject();
        auto destinationNode = AudioNode::Unwrap(jsDestinationNode);
        m_audioContextImpl.connect(destinationNode->m_impl, m_impl);
        return jsDestinationNode;
    }
}

namespace Babylon::Polyfills::WebAudio
{
    void Initialize(Napi::Env env)
    {
        // Set LabSound log level.
        log_set_level(LOGLEVEL_WARN);

        Internal::AudioContext::Initialize(env);
        Internal::AudioParam::Initialize(env);

        auto audioNodeClass = Internal::AudioNode::Initialize(env);
        auto audioScheduledSourceNodeClass = Internal::AudioScheduledSourceNode::Initialize(env);
        auto gainNodeClass = Internal::GainNode::Initialize(env);
        auto oscillatorNodeClass = Internal::OscillatorNode::Initialize(env);

        Napi::Function getPrototypeOf = env.Global().Get("Object").ToObject().Get("getPrototypeOf").As<Napi::Function>();
        Napi::Function setPrototypeOf = env.Global().Get("Object").ToObject().Get("setPrototypeOf").As<Napi::Function>();

        try
        {
            // This works in JavaScriptCore, but throws in Chakra and V8.
            setPrototypeOf.Call({getPrototypeOf.Call({env.Global().Get("GainNode")}), getPrototypeOf.Call({env.Global().Get("AudioNode")})});
            setPrototypeOf.Call({getPrototypeOf.Call({env.Global().Get("OscillatorNode")}), getPrototypeOf.Call({env.Global().Get("AudioNode")})});
        }
        catch (const Napi::Error& error)
        {
            if (error.Message() != "Cyclic __proto__ value")
            {
                throw error;
            }

            // This works in Chakra and V8, but not in JavaScriptCore.
            setPrototypeOf.Call({ audioScheduledSourceNodeClass.Get("prototype"), audioNodeClass.Get("prototype") });
            setPrototypeOf.Call({ audioScheduledSourceNodeClass, audioNodeClass });

            setPrototypeOf.Call({ gainNodeClass.Get("prototype"), audioNodeClass.Get("prototype") });
            setPrototypeOf.Call({ gainNodeClass, audioNodeClass });

            setPrototypeOf.Call({ oscillatorNodeClass.Get("prototype"), audioScheduledSourceNodeClass.Get("prototype") });
            setPrototypeOf.Call({ oscillatorNodeClass, audioScheduledSourceNodeClass });
        }
    }
}
