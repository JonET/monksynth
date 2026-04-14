#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "synth.h"

namespace MonkSynth {

class Processor : public Steinberg::Vst::AudioEffect {
  public:
    Processor();
    ~Processor() override;

    static Steinberg::FUnknown *createInstance(void *) {
        return static_cast<Steinberg::Vst::IAudioProcessor *>(new Processor());
    }

    // AudioEffect overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup &setup) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData &data) override;
    Steinberg::tresult PLUGIN_API
    canProcessSampleSize(Steinberg::int32 symbolicSampleSize) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement *inputs,
                                                     Steinberg::int32 numIns,
                                                     Steinberg::Vst::SpeakerArrangement *outputs,
                                                     Steinberg::int32 numOuts) override;

  private:
    MonkSynthEngine *synth_ = nullptr;
    float paramValues_[19] = {0.5f, 0.5f, 0.8f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f,
                              0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f,
                              0.0f, 0.5f, 0.5f};
    bool xyNoteActive_ = false;
    float xyPendingPitch_ = 0.5f;
    int midiNoteCount_ = 0;
    bool lastNoteActive_ = false;
};

} // namespace MonkSynth
