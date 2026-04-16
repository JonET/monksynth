#include "processor.h"
#include "plugin_cids.h"

#include "pluginterfaces/base/ibstream.h"
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

void Processor::applyParametersToDsp() {
    if (!synth_) return;
    monk_synth_set_glide(synth_, paramValues_[kPortTime]);
    monk_synth_set_vowel(synth_, paramValues_[kVowel]);
    monk_synth_set_delay_mix(synth_, paramValues_[kDelay]);
    monk_synth_set_voice(synth_, paramValues_[kHeadSize]);
    monk_synth_set_vibrato(synth_, paramValues_[kVibrato]);
    monk_synth_set_vibrato_rate(synth_, paramValues_[kVibratoRate]);
    monk_synth_set_aspiration(synth_, paramValues_[kAspiration]);
    monk_synth_set_attack(synth_, paramValues_[kAttack] * 5.0f);
    monk_synth_set_decay(synth_, paramValues_[kDecay] * 5.0f);
    monk_synth_set_sustain(synth_, paramValues_[kSustain]);
    monk_synth_set_release(synth_, paramValues_[kRelease] * 5.0f);
    monk_synth_set_unison(synth_, (int)(paramValues_[kUnison] * 9.0f + 1.5f));
    monk_synth_set_unison_detune(synth_, paramValues_[kUnisonDetune] * 50.0f);
    monk_synth_set_delay_rate(synth_, paramValues_[kDelayRate]);
    monk_synth_set_level(synth_, paramValues_[kLevel]);
    monk_synth_set_unison_voice_spread(synth_, paramValues_[kUnisonVoiceSpread] * 0.5f);
    monk_synth_set_pitch_bend(synth_, (paramValues_[kPitchBend] - 0.5f) * 24.0f);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    if (state) {
        if (!synth_) {
            synth_ = monk_synth_new(static_cast<float>(processSetup.sampleRate));
        }
        applyParametersToDsp();
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

tresult PLUGIN_API Processor::getState(IBStream* state) {
    for (int i = 0; i < kNumParams; i++) {
        float v = paramValues_[i];
        if (state->write(&v, sizeof(v), nullptr) != kResultOk)
            return kResultFalse;
    }
    return kResultOk;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
    // The state format is a raw sequence of kNumParams floats. When the
    // param count grows across a release, older saves are shorter than the
    // current expectation — treat a short read as "end of stored state"
    // and leave the remaining params at their defaults.
    for (int i = 0; i < kNumParams; i++) {
        float v;
        int32 bytesRead = 0;
        tresult r = state->read(&v, sizeof(v), &bytesRead);
        if (r != kResultOk || bytesRead < static_cast<int32>(sizeof(v)))
            break;
        paramValues_[i] = v;
    }
    applyParametersToDsp();
    return kResultOk;
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
                        if (midiNoteCount_ > 0) {
                            // MIDI keys still held: slide back to held note
                            monk_synth_restore_note_stack(synth_);
                        } else {
                            monk_synth_note_off(synth_, 60);
                        }
                    }
                    break;
                case kXYPitchTarget:
                    xyPendingPitch_ = fval;
                    if (xyNoteActive_) {
                        // C3 to C4 (131-262 Hz), one octave
                        float hz = 130.81f * powf(2.0f, fval);
                        monk_synth_set_pitch_hz(synth_, hz);
                    }
                    break;
                case kXYVowel:
                    monk_synth_set_vowel(synth_, fval);
                    break;
                case kPitchBend:
                    // RangeParameter [-12,12]: normalized 0.5 = 0 semitones.
                    // Driven by the in-plugin slider, DAW automation, or the
                    // hardware wheel in Pitch mode. In Both / BothInverted
                    // modes the wheel is routed to kPitchWheelRaw instead,
                    // so this case never needs to touch vowel.
                    monk_synth_set_pitch_bend(synth_, (fval - 0.5f) * 24.0f);
                    break;
                case kPitchBendRouting:
                    // Stored in paramValues_ only; the controller handles
                    // IMidiMapping re-query. No DSP side-effect from here.
                    break;
                case kPitchWheelRaw: {
                    // Hidden hub — only live in Both / BothInverted modes.
                    // Fans out the hardware pitch wheel to pitch bend and
                    // vowel without entangling the user-facing kPitchBend
                    // slider or its automation lane.
                    auto mode = pitchBendModeFromNormalized(paramValues_[kPitchBendRouting]);
                    if (mode != PitchBendMode::Both &&
                        mode != PitchBendMode::BothInverted)
                        break;

                    monk_synth_set_pitch_bend(synth_, (fval - 0.5f) * 24.0f);
                    paramValues_[kPitchBend] = fval;
                    if (data.outputParameterChanges) {
                        int32 pbIndex = 0;
                        auto *pq = data.outputParameterChanges->addParameterData(kPitchBend,
                                                                                  pbIndex);
                        if (pq)
                            pq->addPoint(0, static_cast<ParamValue>(fval), pbIndex);
                    }

                    // Skip the vowel coupling while the XY pad is tracking,
                    // since the pad's smoothed vowel writeback (after the
                    // audio render) would fight this write in the same block.
                    if (!xyNoteActive_) {
                        float vowelVal = (mode == PitchBendMode::BothInverted)
                                             ? (1.0f - fval)
                                             : fval;
                        monk_synth_set_vowel(synth_, vowelVal);
                        paramValues_[kVowel] = vowelVal;
                        if (data.outputParameterChanges) {
                            int32 vIndex = 0;
                            auto *vq =
                                data.outputParameterChanges->addParameterData(kVowel, vIndex);
                            if (vq)
                                vq->addPoint(0, static_cast<ParamValue>(vowelVal), vIndex);
                        }
                    }
                    break;
                }
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
                    if (midiNoteCount_ > 0) midiNoteCount_--;
                    if (xyNoteActive_ && midiNoteCount_ == 0) {
                        // XY pad is held — don't release, just clear the
                        // note stack so XY pad pitch stays in control.
                        monk_synth_note_off(synth_,
                            static_cast<uint8_t>(event.noteOff.pitch));
                        // note_off removed it from the stack; if stack is
                        // now empty the DSP would release — re-assert pitch
                        monk_synth_set_pitch_hz(synth_,
                            130.81f * powf(2.0f, xyPendingPitch_));
                    } else {
                        monk_synth_note_off(synth_,
                            static_cast<uint8_t>(event.noteOff.pitch));
                    }
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

    // --- Send smoothed vowel back to UI so the vowel fader animates the glide ---
    if (xyNoteActive_ && data.outputParameterChanges) {
        int32 index = 0;
        auto* vq = data.outputParameterChanges->addParameterData(kVowel, index);
        if (vq)
            vq->addPoint(0, static_cast<ParamValue>(monk_synth_get_vowel(synth_)), index);
    }

    output.silenceFlags = 0;
    return kResultOk;
}

} // namespace MonkSynth
