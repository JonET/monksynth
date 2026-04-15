/*
 * Private header for MonkSynthEngine internals.
 *
 * This exposes the struct definition and the internal constants so that
 * tests in cpp/tests/ can inspect and assert on the engine's state. It
 * is NOT part of the public DSP API — the plugin shell should continue
 * to use synth.h only.
 */

#ifndef MONK_SYNTH_INTERNAL_H
#define MONK_SYNTH_INTERNAL_H

#include "delay.h"
#include "synth.h"
#include "voice.h"

#define MONK_MAX_BUF 8192
#define MONK_MAX_NOTES 16
#define MONK_MAX_UNISON 10

typedef struct {
    uint8_t note;
    float velocity;
} MonkHeldNote;

struct MonkSynthEngine {
    MonkVoice voices[MONK_MAX_UNISON];
    int unison_count;
    float unison_detune;       /* max detune spread in cents */
    float unison_voice_spread; /* voice character spread across unison (0-1) */
    float base_voice;          /* center voice value before spread */
    float last_base_hz;        /* last note frequency for live detune updates */

    MonkDelay delay;

    MonkHeldNote held[MONK_MAX_NOTES];
    uint32_t held_count;
    float cc_volume;
    float level;              /* output level 0-1 (GUI knob) */
    float current_voice_gain; /* smoothed unison gain */
    float target_voice_gain;

    float scratch_mono[MONK_MAX_BUF];
    float scratch_voice[MONK_MAX_BUF];
    float scratch_l[MONK_MAX_BUF];
    float scratch_r[MONK_MAX_BUF];
};

#endif
