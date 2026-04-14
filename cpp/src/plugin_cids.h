#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace MonkSynth {

// Processor component GUID  (MNKS-PROC-MNKS-YNTH)
static const Steinberg::FUID kProcessorUID(0x4D4E4B53, 0x50524F43, 0x4D4E4B53, 0x594E5448);

// Controller component GUID (MNKS-CTRL-MNKS-YNTH)
static const Steinberg::FUID kControllerUID(0x4D4E4B53, 0x4354524C, 0x4D4E4B53, 0x594E5448);

// Parameter IDs
enum : Steinberg::Vst::ParamID {
    kPortTime = 0,
    kVowel = 1,
    kDelay = 2,
    kHeadSize = 3,
    kVibrato = 4,
    kVibratoRate = 5,
    kAspiration = 6,
    kAttack = 7,
    kDecay = 8,
    kSustain = 9,
    kRelease = 10,
    kUnison = 11,
    kUnisonDetune = 12,
    kDelayRate = 13,
    kLevel = 14,
    kUnisonVoiceSpread = 15,
    kXYNoteOn = 16,       // 1.0 = note on, 0.0 = note off
    kXYVowel = 17,        // target vowel from XY pad Y axis
    kXYPitchTarget = 18,  // target pitch from XY pad X axis
    kNumParams = 19,

    // Private parameters (not exposed to host automation)
    kXYPitch = 101,       // smoothed pitch for indicator display

    // Output parameter: processor -> controller (for monk animation)
    kNoteActive = 200, // 1.0 when any note is sounding, 0.0 when silent
};

} // namespace MonkSynth
