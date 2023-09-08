#include <Babylon/JsRuntime.h>

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
                    InstanceMethod("createGain", &AudioContext::CreateGain)
                });

            env.Global().Set(JS_CLASS_NAME, func);
        }

        AudioContext(const Napi::CallbackInfo& info)
            : Napi::ObjectWrap<AudioContext>{info}
            , m_runtime{JsRuntime::GetFromJavaScript(info.Env())}
            , m_deviceImpl{std::make_shared<lab::AudioDevice_RtAudio>(lab::AudioStreamConfig(), GetDefaultAudioDeviceConfiguration())}
            , m_impl{std::make_shared<lab::AudioContext>(false, true)}
        {
            auto destinationNode = std::make_shared<lab::AudioDestinationNode>(*m_impl.get(), m_deviceImpl);
            m_deviceImpl->setDestinationNode(destinationNode);
            m_impl->setDestinationNode(destinationNode);
        }

        lab::AudioContext& impl() const { return *m_impl; }

    private:
        Napi::Value CreateGain(const Napi::CallbackInfo& info);

        JsRuntime& m_runtime;
        std::shared_ptr<lab::AudioDevice_RtAudio> m_deviceImpl;
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
