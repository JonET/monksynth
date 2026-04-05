#include "processor.h"
#include "plugin_cids.h"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <cmath>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace MonkSynth {

Processor::Processor() {
    setControllerClass(kControllerUID);
}

Processor::~Processor() {
    if (synth_) {
        monk_synth_free(synth_);
        synth_ = nullptr;
    }
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;

    // No audio input — this is a synthesizer
    addEventInput(STR16("MIDI In"));
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

    return kResultOk;
}

tresult PLUGIN_API Processor::terminate() {
    if (synth_) {
        monk_synth_free(synth_);
        synth_ = nullptr;
    }
    return AudioEffect::terminate();
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    if (state) {
        if (!synth_) {
            synth_ = monk_synth_new(static_cast<float>(processSetup.sampleRate));
        }
        // Apply current parameter values
        monk_synth_set_glide(synth_, paramValues_[kPortTime]);
        monk_synth_set_vowel(synth_, paramValues_[kVowel]);
        monk_synth_set_delay_mix(synth_, paramValues_[kDelay]);
        monk_synth_set_voice(synth_, paramValues_[kHeadSize]);
        monk_synth_set_vibrato(synth_, paramValues_[kVibrato]);
        monk_synth_set_vibrato_rate(synth_, paramValues_[kVibratoRate]);
        monk_synth_set_aspiration(synth_, paramValues_[kAspiration]);
        monk_synth_set_attack(synth_, paramValues_[kAttack] * 3.0f);
        monk_synth_set_decay(synth_, paramValues_[kDecay] * 3.0f);
        monk_synth_set_sustain(synth_, paramValues_[kSustain]);
        monk_synth_set_release(synth_, paramValues_[kRelease] * 3.0f);
        monk_synth_set_level(synth_, paramValues_[kLevel]);
    } else {
        if (synth_) {
            monk_synth_reset(synth_);
        }
    }
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    if (synth_) {
        monk_synth_set_sample_rate(synth_, static_cast<float>(setup.sampleRate));
    }
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize) {
    // Only 32-bit float
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Processor::setBusArrangements(
    SpeakerArrangement* inputs, int32 numIns,
    SpeakerArrangement* outputs, int32 numOuts)
{
    // No inputs, stereo output only
    if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo) {
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    }
    return kResultFalse;
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    if (!synth_) return kResultOk;

    // --- Handle parameter changes ---
    if (data.inputParameterChanges) {
        int32 numParams = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParams; i++) {
            auto* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;

            auto id = queue->getParameterId();
            int32 numPoints = queue->getPointCount();
            if (numPoints <= 0) continue;

            // Use the last value in the queue
            ParamValue value;
            int32 sampleOffset;
            queue->getPoint(numPoints - 1, sampleOffset, value);

            float fval = static_cast<float>(value);
            if (id < kNumParams) {
                paramValues_[id] = fval;
            }

            switch (id) {
                case kPortTime:  monk_synth_set_glide(synth_, fval); break;
                case kVowel:     monk_synth_set_vowel(synth_, fval); break;
                case kDelay:     monk_synth_set_delay_mix(synth_, fval); break;
                case kHeadSize:  monk_synth_set_voice(synth_, fval); break;
                case kVibrato:    monk_synth_set_vibrato(synth_, fval); break;
                case kVibratoRate: monk_synth_set_vibrato_rate(synth_, fval); break;
                case kAspiration:  monk_synth_set_aspiration(synth_, fval); break;
                case kAttack:      monk_synth_set_attack(synth_, fval * 5.0f); break;
                case kDecay:       monk_synth_set_decay(synth_, fval * 5.0f); break;
                case kSustain:     monk_synth_set_sustain(synth_, fval); break;
                case kRelease:      monk_synth_set_release(synth_, fval * 5.0f); break;
                case kUnison:       monk_synth_set_unison(synth_, (int)(fval * 9.0f + 1.5f)); break;
                case kUnisonDetune: monk_synth_set_unison_detune(synth_, fval * 50.0f); break;
                case kDelayRate:    monk_synth_set_delay_rate(synth_, fval); break;
                case kLevel:        monk_synth_set_level(synth_, fval); break;
                case kUnisonVoiceSpread: monk_synth_set_unison_voice_spread(synth_, fval * 0.5f); break;
                case kXYNoteOn:
                    if (fval > 0.5f) {
                        xyNoteActive_ = true;
                        float startHz = 130.81f * powf(2.0f, xyPendingPitch_);
                        monk_synth_set_pitch_hz(synth_, startHz);
                    } else {
                        xyNoteActive_ = false;
                        monk_synth_note_off(synth_, 60);
                    }
                    break;
                case kXYPitch:
                    xyPendingPitch_ = fval;
                    if (xyNoteActive_) {
                        // C3 to C4 (131-262 Hz), one octave
                        float hz = 130.81f * powf(2.0f, fval);
                        monk_synth_set_pitch_hz(synth_, hz);
                    }
                    break;
                default: break;
            }
        }
    }

    // --- Handle MIDI events ---
    if (data.inputEvents) {
        int32 numEvents = data.inputEvents->getEventCount();
        for (int32 i = 0; i < numEvents; i++) {
            Event event;
            if (data.inputEvents->getEvent(i, event) != kResultOk) continue;

            switch (event.type) {
                case Event::kNoteOnEvent:
                    monk_synth_note_on(synth_,
                        static_cast<uint8_t>(event.noteOn.pitch),
                        event.noteOn.velocity);
                    midiNoteCount_++;
                    break;
                case Event::kNoteOffEvent:
                    monk_synth_note_off(synth_,
                        static_cast<uint8_t>(event.noteOff.pitch));
                    if (midiNoteCount_ > 0) midiNoteCount_--;
                    break;
                default:
                    break;
            }
        }
    }

    // --- Process audio ---
    if (data.numOutputs < 1 || data.numSamples == 0) return kResultOk;

    auto& output = data.outputs[0];
    float* outL = output.channelBuffers32[0];
    float* outR = output.channelBuffers32[1];

    monk_synth_process(synth_, outL, outR, static_cast<uint32_t>(data.numSamples));

    // --- Send note-held state to controller for monk animation ---
    bool active = (midiNoteCount_ > 0 || xyNoteActive_);
    if (active != lastNoteActive_) {
        lastNoteActive_ = active;
        if (data.outputParameterChanges) {
            int32 index = 0;
            auto* queue = data.outputParameterChanges->addParameterData(kNoteActive, index);
            if (queue) {
                queue->addPoint(0, active ? 1.0 : 0.0, index);
            }
        }
    }

    output.silenceFlags = 0;
    return kResultOk;
}

} // namespace MonkSynth
