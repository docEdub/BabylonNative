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

    std::shared_ptr<lab::AudioDevice_Miniaudio> GetDefaultAudioDevice()
    {
        static auto device = std::make_shared<lab::AudioDevice_Miniaudio>(lab::AudioStreamConfig(), GetDefaultAudioDeviceConfiguration());
        return device;
    }

    class NativeAudioContext final : public Napi::ObjectWrap<NativeAudioContext>
    {
        static constexpr auto JS_CLASS_NAME = "NativeAudioContext";

    public:
        static void Initialize(Napi::Env env)
        {
            auto func = NativeAudioContext::DefineClass(
                env,
                JS_CLASS_NAME,
                {
                    InstanceAccessor("destination", &NativeAudioContext::GetDestination, nullptr),
                    InstanceMethod("createGain", &NativeAudioContext::CreateGain)
                });
            env.Global().Set(JS_CLASS_NAME, func);
        }

        NativeAudioContext(const Napi::CallbackInfo& info);

        lab::AudioContext& Impl() const { return *m_impl; }

    private:
        Napi::Value GetDestination(const Napi::CallbackInfo& info)
        {
            assert(m_jsDestinationNode.Value().IsObject());
            return m_jsDestinationNode.Value();
        }

        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        std::shared_ptr<lab::AudioDevice_Miniaudio> m_deviceImpl;
        std::shared_ptr<lab::AudioContext> m_impl;
        std::shared_ptr<lab::AudioDestinationNode> m_destinationNodeImpl;

        Napi::ObjectReference m_js;
        Napi::ObjectReference m_jsDestinationNode;
    };

    class AudioParam final : public Napi::ObjectWrap<AudioParam>
    {
        static constexpr auto JS_CLASS_NAME = "NativeAudioParam";

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
            : m_js{Napi::Persistent(info[0].ToObject())}
            , m_audioContextImpl{NativeAudioContext::Unwrap(info[1].ToObject())->Impl()} // TODO: Why is info[1] not an object?
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

        Napi::ObjectReference m_js;
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

    class NativeAudioNode final : public AudioNodeWrap<NativeAudioNode>
    {
        static constexpr auto JS_CLASS_NAME = "NativeAudioNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                Properties(
                ));
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext, std::shared_ptr<lab::AudioNode> impl)
        {
            auto jsAudioNode = info.Env().Global().Get("AudioNode").As<Napi::Function>().New({audioContext});
            auto audioNode = NativeAudioNode::Unwrap(jsAudioNode.Get("_native").ToObject());
            audioNode->SetImpl(impl);
            return jsAudioNode;
            //return info.Env().Global().Get("Object").As<Napi::Function>().New({});
        }

        NativeAudioNode(const Napi::CallbackInfo& info)
            : AudioNodeWrap<NativeAudioNode>(info)
        {
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

    class NativeGainNode final : public Napi::ObjectWrap<NativeGainNode>
    {
        static constexpr auto JS_CLASS_NAME = "NativeGainNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                {
                    InstanceAccessor("gain", &NativeGainNode::GetGain, nullptr)
                });
                //));
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({audioContext});
        }

        NativeGainNode(const Napi::CallbackInfo& info)
            : ObjectWrap<NativeGainNode>{info}
        {
            ////SetImpl(std::make_shared<lab::GainNode>(m_audioContextImpl));
            ////m_jsGain = Napi::Persistent(AudioParam::New(info, Impl()->gain()));
        }

    private:
        Napi::Value GetGain(const Napi::CallbackInfo& info)
        {
            return m_jsGain.Value();
        }

        //std::shared_ptr<lab::GainNode> Impl() const
        //{
        //    return AudioNodeBase::Impl<lab::GainNode>();
        //}

        Napi::ObjectReference m_jsGain;
    };

    class NativeOscillatorNode final : public AudioScheduledSourceNodeWrap<NativeOscillatorNode>
    {
        static constexpr auto JS_CLASS_NAME = "NativeOscillatorNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(
                env,
                JS_CLASS_NAME,
                Properties(
                    InstanceAccessor("frequency", &NativeOscillatorNode::GetFrequency, nullptr)
                ));
            env.Global().Set(JS_CLASS_NAME, func);
            return func;
        }

        static Napi::Object New(const Napi::CallbackInfo& info, Napi::Value audioContext)
        {
            return info.Env().Global().Get(JS_CLASS_NAME).As<Napi::Function>().New({audioContext});
        }

        NativeOscillatorNode(const Napi::CallbackInfo& info)
            : AudioScheduledSourceNodeWrap<NativeOscillatorNode>{info}
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

    NativeAudioContext::NativeAudioContext(const Napi::CallbackInfo& info)
        : ObjectWrap<NativeAudioContext>{info}
        , m_deviceImpl{GetDefaultAudioDevice()}
        , m_impl{std::make_shared<lab::AudioContext>(false, true)}
        , m_destinationNodeImpl{std::make_shared<lab::AudioDestinationNode>(*m_impl.get(), m_deviceImpl)}
        , m_js(Napi::Persistent(info[0].ToObject())) // TODO: Is this a circular reference that keeps the GC from freeing it?
        , m_jsDestinationNode{Napi::Persistent(NativeAudioNode::New(info, info.This(), m_destinationNodeImpl))}
    {
        m_deviceImpl->setDestinationNode(m_destinationNodeImpl);
        m_impl->setDestinationNode(m_destinationNodeImpl);
    }

    Napi::Value NativeAudioContext::CreateGain(const Napi::CallbackInfo& info)
    {
        return NativeGainNode::New(info, info.This());
    }

    Napi::Value AudioNodeBase::Connect(const Napi::CallbackInfo& info)
    {
        auto jsDestinationNode = info[0].ToObject();
        auto destinationNode = NativeAudioNode::Unwrap(jsDestinationNode);
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

        Internal::NativeAudioContext::Initialize(env);
        Internal::NativeAudioNode::Initialize(env);

        env.Global().Get("eval").As<Napi::Function>().Call({Napi::Value::From(env, R"(

class EventTarget {
    constructor() {
    }
}

class BaseAudioContext extends EventTarget {
    constructor() {
        super();
    }
}

class AudioContext extends BaseAudioContext {
    constructor() {
        super();
        this._native = new NativeAudioContext(this);
    }

    createGain() {
        return new GainNode(this);
    }
}

class AudioNode extends EventTarget {
    constructor(audioContext) {
        super();
        this._native = new NativeAudioNode(this, audioContext._native); // TODO: audioContext._native isn't defined, yet, when AudioContext constructs and starts creating it's destination node.
    }

    connect(node) {
        return node;
    }
}

class GainNode extends AudioNode {
    constructor(audioContext) {
        super(audioContext);
    }
}

window.EventTarget = EventTarget;
window.BaseAudioContext = BaseAudioContext;
window.AudioContext = AudioContext;
window.AudioNode = AudioNode;
window.GainNode = GainNode;

            )")});
    }
}
