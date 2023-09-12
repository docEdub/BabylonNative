#include <LabSound/LabSound.h>
#include <LabSound/backends/AudioDevice_RtAudio.h>

#include <napi/napi.h>

namespace Babylon::Polyfills::Internal
{
    lab::AudioStreamConfig GetDefaultAudioDeviceConfiguration()
    {
        const std::vector<lab::AudioDeviceInfo> audioDevices = lab::AudioDevice_RtAudio::MakeAudioDeviceList();

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
                    InstanceAccessor("destination", &AudioContext::GetDestination, nullptr),
                    InstanceMethod("createGain", &AudioContext::CreateGain)
                });

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info);

        lab::AudioContext& Impl() const { return *m_impl; }

    private:
        Napi::Value GetDestination(const Napi::CallbackInfo& info)
        {
            assert(m_jsDestinationNode.Value().IsObject());
            return m_jsDestinationNode.Value();
        }

        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        std::shared_ptr<lab::AudioDevice_RtAudio> m_deviceImpl;
        std::shared_ptr<lab::AudioContext> m_impl;
        std::shared_ptr<lab::AudioDestinationNode> m_destinationNodeImpl;

        Napi::ObjectReference m_jsDestinationNode;
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
            return std::reinterpret_pointer_cast<ImplT>(m_impl);
        }

        void SetImpl(std::shared_ptr<lab::AudioNode> impl)
        {
            m_impl = std::move(impl);
        }

        lab::AudioContext& m_audioContextImpl;
        std::shared_ptr<lab::AudioNode> m_impl;
    };

    template<class T>
    class AudioNodeWrap : public Napi::ObjectWrap<T>, public AudioNodeBase
    {
    public:
        AudioNodeWrap(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<T>(info)
            , AudioNodeBase(info)
        {
        }

    protected:
        template<typename ...Args>
        static const std::initializer_list<Napi::ClassPropertyDescriptor<T>>& Properties(Args&&... args)
        {
            static std::initializer_list<Napi::ClassPropertyDescriptor<T>> properties = {
                Napi::ObjectWrap<T>::InstanceMethod("connect", &AudioNodeBase::Connect),
                args ...
            };
            return properties;
        }
    };

    class AudioNode final : public AudioNodeWrap<AudioNode>
    {
        static constexpr auto JS_CLASS_NAME = "AudioNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(env, JS_CLASS_NAME, Properties());
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
    };

    class GainNode final : public AudioNodeWrap<GainNode>
    {
        static constexpr auto JS_CLASS_NAME = "GainNode";

    public:
        static Napi::Function Initialize(Napi::Env env)
        {
            Napi::Function func = DefineClass(env, JS_CLASS_NAME, Properties());
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
        }
    };

    AudioContext::AudioContext(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<AudioContext>{info}
        , m_deviceImpl{std::make_shared<lab::AudioDevice_RtAudio>(lab::AudioStreamConfig(), GetDefaultAudioDeviceConfiguration())}
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
        // Silence LabSound output.
        log_set_quiet(true);

        Internal::AudioContext::Initialize(env);
        auto audioNodeClass = Internal::AudioNode::Initialize(env);
        auto gainNodeClass = Internal::GainNode::Initialize(env);

        Napi::Function setPrototypeOf = env.Global().Get("Object").ToObject().Get("setPrototypeOf").As<Napi::Function>();

        try
        {
            // TODO: Fix error on JavaScriptCore -> Uncaught Error: Cannot set prototype of immutable prototype object.
            // ... and find out why `gainNode instanceof AudioNode` is still true even though the prototype chain is not set correctly here on JSC.
            setPrototypeOf.Call({ gainNodeClass.Get("prototype"), audioNodeClass.Get("prototype") });
            setPrototypeOf.Call({ gainNodeClass, audioNodeClass });
        }
        catch(...)
        {
        }
    }
}
