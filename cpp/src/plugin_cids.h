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
    kPitchBend = 19,      // ±12 semitones, default 0
    kPitchBendRouting = 20, // 4-step discrete: Classic / Both / BothInv / Pitch — hidden
    kNumParams = 21,

    // Output parameter: processor -> controller (for monk animation)
    kNoteActive = 200, // 1.0 when any note is sounding, 0.0 when silent
};

// How the hardware pitch wheel (and the kPitchBend parameter itself) is
// routed. Ordering is chosen so that the Classic (0.0) and Pitch (1.0)
// endpoints stay backward-compatible with pre-existing saved state that
// only knew about the binary toggle.
enum class PitchBendMode : int {
    Classic = 0,      // wheel -> Vowel (Delay Lama compat)
    Both = 1,         // wheel -> PitchBend + Vowel
    BothInverted = 2, // wheel -> PitchBend + (1 - Vowel)
    Pitch = 3,        // wheel -> PitchBend only
};

inline PitchBendMode pitchBendModeFromNormalized(float v) {
    int i = static_cast<int>(v * 3.0f + 0.5f);
    if (i < 0)
        i = 0;
    if (i > 3)
        i = 3;
    return static_cast<PitchBendMode>(i);
}

inline float pitchBendModeToNormalized(PitchBendMode m) {
    return static_cast<float>(static_cast<int>(m)) / 3.0f;
}

} // namespace MonkSynth
