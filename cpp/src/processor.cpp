#include "processor.h"
#include "plugin_cids.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
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
    vowelFromXY_ = false;
    lastSentVowel_ = paramValues_[kVowel];
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

void Processor::applyParameter(ParamID id, float fval, int32 offset, ProcessData& data) {
    if (id < kNumParams) {
        paramValues_[id] = fval;
    }

    switch (id) {
        case kPortTime:  monk_synth_set_glide(synth_, fval); break;
        case kVowel:
            monk_synth_set_vowel(synth_, fval);
            vowelFromXY_ = false;
            lastSentVowel_ = fval;
            break;
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
            vowelFromXY_ = true;
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
                auto *pq = data.outputParameterChanges->addParameterData(kPitchBend, pbIndex);
                if (pq)
                    pq->addPoint(offset, static_cast<ParamValue>(fval), pbIndex);
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
                vowelFromXY_ = false;
                lastSentVowel_ = vowelVal;
                if (data.outputParameterChanges) {
                    int32 vIndex = 0;
                    auto *vq = data.outputParameterChanges->addParameterData(kVowel, vIndex);
                    if (vq)
                        vq->addPoint(offset, static_cast<ParamValue>(vowelVal), vIndex);
                }
            }
            break;
        }
        default: break;
    }
}

void Processor::applyTimelinePoint(const TimelinePoint& p, ProcessData& data) {
    switch (p.kind) {
        case TimelinePoint::Kind::Param:
            applyParameter(p.id, p.value, std::max<int32>(p.offset, 0), data);
            break;
        case TimelinePoint::Kind::NoteOn:
            monk_synth_note_on(synth_, p.pitch, p.value);
            midiNoteCount_++;
            break;
        case TimelinePoint::Kind::NoteOff:
            if (midiNoteCount_ > 0) midiNoteCount_--;
            if (xyNoteActive_ && midiNoteCount_ == 0) {
                // XY pad is held — don't release, just clear the
                // note stack so XY pad pitch stays in control.
                monk_synth_note_off(synth_, p.pitch);
                // note_off removed it from the stack; if stack is
                // now empty the DSP would release — re-assert pitch
                monk_synth_set_pitch_hz(synth_,
                    130.81f * powf(2.0f, xyPendingPitch_));
            } else {
                monk_synth_note_off(synth_, p.pitch);
            }
            break;
    }
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    if (!synth_) return kResultOk;

    // --- Output buffers ---
    // The bus may be present but inactive (null channel pointers); events
    // and parameters must still be applied so state stays in sync.
    const int32 numSamples = data.numSamples;
    float* outL = nullptr;
    float* outR = nullptr;
    if (data.numOutputs >= 1 && data.outputs[0].numChannels >= 2 &&
        data.outputs[0].channelBuffers32) {
        outL = data.outputs[0].channelBuffers32[0];
        outR = data.outputs[0].channelBuffers32[1];
    }

    int32 rendered = 0;
    auto renderTo = [&](int32 upTo) {
        upTo = std::min(upTo, numSamples);
        if (outL && outR && upTo > rendered) {
            monk_synth_process(synth_, outL + rendered, outR + rendered,
                               static_cast<uint32_t>(upTo - rendered));
            rendered = upTo;
        }
    };

    // --- Merge parameter points and note events into one timeline ---
    // Both are timestamped within the block. Applying them all up front and
    // then rendering the whole block started every note (and bend, and XY
    // pad move) at the block boundary, up to one buffer early with the
    // error varying from note to note (#22). Instead: sort everything by
    // offset, and render up to each point before applying it. At equal
    // offsets parameters are applied before notes, as before.
    int count = 0;
    auto push = [&](TimelinePoint p) {
        if (count < kMaxTimeline) {
            p.seq = count;
            timeline_[count++] = p;
        } else {
            // Pathological event density; apply immediately rather than
            // drop it. Ordering is only approximate past this point.
            renderTo(p.offset);
            applyTimelinePoint(p, data);
        }
    };

    if (data.inputParameterChanges) {
        int32 numParams = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParams; i++) {
            auto* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;
            ParamID id = queue->getParameterId();
            int32 numPoints = queue->getPointCount();
            for (int32 j = 0; j < numPoints; j++) {
                int32 sampleOffset = 0;
                ParamValue value = 0.0;
                if (queue->getPoint(j, sampleOffset, value) != kResultOk) continue;
                TimelinePoint p;
                p.kind = TimelinePoint::Kind::Param;
                p.offset = sampleOffset;
                p.id = id;
                p.value = static_cast<float>(value);
                push(p);
            }
        }
    }

    if (data.inputEvents) {
        int32 numEvents = data.inputEvents->getEventCount();
        for (int32 i = 0; i < numEvents; i++) {
            Event event;
            if (data.inputEvents->getEvent(i, event) != kResultOk) continue;
            // Only note on/off affect the synth; other event kinds (note
            // expression, poly pressure, sysex) must not split the render.
            TimelinePoint p;
            p.offset = event.sampleOffset;
            if (event.type == Event::kNoteOnEvent) {
                p.kind = TimelinePoint::Kind::NoteOn;
                p.pitch = static_cast<uint8>(event.noteOn.pitch);
                p.value = event.noteOn.velocity;
            } else if (event.type == Event::kNoteOffEvent) {
                p.kind = TimelinePoint::Kind::NoteOff;
                p.pitch = static_cast<uint8>(event.noteOff.pitch);
            } else {
                continue;
            }
            push(p);
        }
    }

    std::sort(timeline_, timeline_ + count, [](const TimelinePoint& a, const TimelinePoint& b) {
        return a.offset != b.offset ? a.offset < b.offset : a.seq < b.seq;
    });

    // --- Render, applying each point at its offset ---
    for (int i = 0; i < count; i++) {
        renderTo(timeline_[i].offset);
        applyTimelinePoint(timeline_[i], data);
    }

    if (data.numOutputs < 1 || numSamples == 0) return kResultOk;

    renderTo(numSamples);

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

    // --- Send smoothed vowel back to UI so the face and vowel fader follow ---
    // Covers both the XY pad and XY Vowel automation during MIDI notes; the
    // controller only sees kXYVowel, which neither the face nor the fader
    // track. Skipped when kVowel drove the change so a knob drag with glide
    // on isn't fought by the lagging engine value.
    if ((xyNoteActive_ || vowelFromXY_) && data.outputParameterChanges) {
        float vowel = monk_synth_get_vowel(synth_);
        if (vowel != lastSentVowel_) {
            int32 index = 0;
            auto* vq = data.outputParameterChanges->addParameterData(kVowel, index);
            if (vq && vq->addPoint(0, static_cast<ParamValue>(vowel), index) == kResultOk)
                lastSentVowel_ = vowel;
        }
    }

    data.outputs[0].silenceFlags = 0;
    return kResultOk;
}

} // namespace MonkSynth
