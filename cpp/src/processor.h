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
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state) override;

  private:
    void applyParametersToDsp();

    // One parameter point or note event, positioned within the current
    // block. process() merges both sources into a single sample-ordered
    // timeline so everything lands at its exact offset.
    struct TimelinePoint {
        enum class Kind : Steinberg::uint8 { Param, NoteOn, NoteOff };
        Steinberg::int32 offset = 0;
        Steinberg::int32 seq = 0; // arrival order, breaks ties at equal offset
        Kind kind = Kind::Param;
        Steinberg::Vst::ParamID id = 0; // Param
        float value = 0.0f;             // Param: normalized value; NoteOn: velocity
        Steinberg::uint8 pitch = 0;     // NoteOn / NoteOff
    };
    static constexpr int kMaxTimeline = 1024;

    void applyParameter(Steinberg::Vst::ParamID id, float value, Steinberg::int32 offset,
                        Steinberg::Vst::ProcessData &data);
    void applyTimelinePoint(const TimelinePoint &p, Steinberg::Vst::ProcessData &data);

    MonkSynthEngine *synth_ = nullptr;
    // Order must match the ParamID enum in plugin_cids.h.
    // PitchBend (idx 19): 0.5 = 0 semitones (RangeParameter midpoint).
    // PitchBendRouting (idx 20): 0.0 = Classic (Vowel), preserves Delay Lama.
    // PitchWheelRaw (idx 21): 0.5 = wheel-at-rest; only used in Both modes.
    float paramValues_[22] = {0.5f, 0.5f, 0.8f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f,
                              0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f,
                              0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f};
    bool xyNoteActive_ = false;
    float xyPendingPitch_ = 0.5f;
    int midiNoteCount_ = 0;
    bool lastNoteActive_ = false;
    // True while the engine's vowel was last set by kXYVowel (pad or its
    // automation lane). The face and Vowel fader only follow kVowel, so the
    // processor echoes the engine vowel back as kVowel in that case.
    bool vowelFromXY_ = false;
    float lastSentVowel_ = 0.5f; // kVowel as the controller last saw it
    TimelinePoint timeline_[kMaxTimeline];
};

} // namespace MonkSynth
