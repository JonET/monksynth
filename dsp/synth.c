/*
 * Top-level synth API — glues together the voice(s) and delay, manages the
 * monophonic note stack, routes MIDI CCs to parameters, and handles
 * gain staging. This is the only file external code needs to call.
 *
 * Unison mode runs multiple detuned copies of the voice on the same note,
 * creating a thicker choir-like sound. Count=1 is a single voice.
 */

#include "synth.h"
#include "delay.h"
#include "voice.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUF 8192
#define MAX_NOTES 16
#define MAX_UNISON 10

typedef struct {
    uint8_t note;
    float velocity;
} HeldNote;

struct MonkSynthEngine {
    MonkVoice voices[MAX_UNISON];
    int unison_count;
    float unison_detune;       /* max detune spread in cents */
    float unison_voice_spread; /* voice character spread across unison (0-1) */
    float base_voice;          /* center voice value before spread */
    float last_base_hz;        /* last note frequency for live detune updates */

    MonkDelay delay;

    HeldNote held[MAX_NOTES];
    uint32_t held_count;
    float cc_volume;
    float level;              /* output level 0-1 (GUI knob) */
    float current_voice_gain; /* smoothed unison gain */
    float target_voice_gain;

    float scratch_mono[MAX_BUF];
    float scratch_voice[MAX_BUF]; /* per-voice temp buffer */
    float scratch_l[MAX_BUF];
    float scratch_r[MAX_BUF];
};

/* Remove a note from the held-note stack, compacting in place. */
static void remove_note(MonkSynthEngine *s, uint8_t note) {
    uint32_t j = 0;
    for (uint32_t i = 0; i < s->held_count; i++) {
        if (s->held[i].note != note)
            s->held[j++] = s->held[i];
    }
    s->held_count = j;
}

/* Apply a function to all active unison voices. */
#define FOR_VOICES(s, call)                                                                        \
    for (int _i = 0; _i < (s)->unison_count; _i++)                                                 \
    call(&(s)->voices[_i])

/* ---- Lifecycle ---- */

MonkSynthEngine *monk_synth_new(float sample_rate) {
    MonkSynthEngine *s = calloc(1, sizeof(MonkSynthEngine));
    if (!s)
        return NULL;

    s->unison_count = 1;
    s->unison_detune = 0.0f;
    s->current_voice_gain = 1.0f;
    s->target_voice_gain = 1.0f;

    for (int i = 0; i < MAX_UNISON; i++) {
        monk_voice_init(&s->voices[i], sample_rate);
        /* Stagger vibrato phase and RNG seed so unison voices don't move
         * in lockstep — each one sounds like an independent singer. */
        s->voices[i].vibrato_phase = (float)(MONK_SINE_TBL_SIZE * i) / MAX_UNISON;
        s->voices[i].rng_state = 12345 + i * 7919;
    }

    monk_delay_init(&s->delay, sample_rate);
    s->cc_volume = 0.1f;
    s->level = 1.0f;

    /* Rabten preset defaults */
    monk_synth_set_glide(s, 0.5f);
    monk_synth_set_vowel(s, 0.5f);
    monk_synth_set_voice(s, 0.5f);
    monk_delay_set_mix(&s->delay, 0.8f);

    return s;
}

void monk_synth_free(MonkSynthEngine *s) { free(s); }

void monk_synth_set_sample_rate(MonkSynthEngine *s, float sample_rate) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_sample_rate(&s->voices[i], sample_rate);
    monk_delay_set_sample_rate(&s->delay, sample_rate);
}

void monk_synth_reset(MonkSynthEngine *s) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_reset(&s->voices[i]);
    monk_delay_reset(&s->delay);
    s->held_count = 0;
    s->cc_volume = 0.1f;
}

/* ---- Helpers ---- */

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* Apply voice character spread across unison voices. Each voice gets a
 * symmetric offset from the base voice value, clamped to 0-1. */
static void apply_voice_spread(MonkSynthEngine *s) {
    int n = s->unison_count;
    for (int i = 0; i < n; i++) {
        float offset = 0.0f;
        if (n > 1)
            offset = s->unison_voice_spread * ((float)i / (float)(n - 1) * 2.0f - 1.0f);
        monk_voice_set_voice(&s->voices[i], clamp01(s->base_voice + offset));
    }
}

/* ---- Note control ---- */

/* Compute frequency for a single unison voice with detune spread. */
static float detuned_hz(float base_hz, float detune_cents, int voice_idx, int voice_count) {
    if (voice_count <= 1)
        return base_hz;
    float offset = detune_cents * ((float)voice_idx / (float)(voice_count - 1) * 2.0f - 1.0f);
    return base_hz * powf(2.0f, offset / 1200.0f);
}

/* Apply detuning spread across unison voices. Voice 0 is centered,
 * others spread symmetrically in cents: -detune ... 0 ... +detune */
static void apply_unison_detune(MonkSynthEngine *s, float base_hz) {
    s->last_base_hz = base_hz;
    int n = s->unison_count;
    for (int i = 0; i < n; i++) {
        float hz = detuned_hz(base_hz, s->unison_detune, i, n);
        monk_voice_note_on(&s->voices[i], hz, 1.0f);
    }
}

/* Monophonic note stack: last-note priority. When multiple keys are held,
 * the most recent note sounds. Releasing it falls back to the previous. */
void monk_synth_note_on(MonkSynthEngine *s, uint8_t note, float velocity) {
    if (!s)
        return;
    /* Clear XY pad smoothing — MIDI uses the portamento knob only */
    for (int i = 0; i < MAX_UNISON; i++)
        s->voices[i].min_glide = 0.0f;
    remove_note(s, note);
    if (s->held_count < MAX_NOTES) {
        s->held[s->held_count].note = note;
        s->held[s->held_count].velocity = velocity;
        s->held_count++;
    }
    apply_unison_detune(s, monk_midi_note_to_freq(note));
}

void monk_synth_note_off(MonkSynthEngine *s, uint8_t note) {
    if (!s)
        return;
    remove_note(s, note);
    if (s->held_count > 0) {
        uint8_t top = s->held[s->held_count - 1].note;
        apply_unison_detune(s, monk_midi_note_to_freq(top));
    } else {
        for (int i = 0; i < MAX_UNISON; i++)
            monk_voice_note_off(&s->voices[i]);
    }
}

/* Direct pitch control for the XY pad. Only sets the target pitch —
 * min_glide ensures smooth slewing independent of the portamento knob,
 * providing smooth slewing for mouse/touch input. */
#define XY_PAD_GLIDE 0.3f
void monk_synth_set_pitch_hz(MonkSynthEngine *s, float hz) {
    if (!s)
        return;
    s->last_base_hz = hz;
    if (!monk_voice_is_active(&s->voices[0]))
        apply_unison_detune(s, hz);
    int n = s->unison_count;
    for (int i = 0; i < n; i++) {
        s->voices[i].min_glide = XY_PAD_GLIDE;
        monk_voice_set_pitch_target(&s->voices[i], detuned_hz(hz, s->unison_detune, i, n));
    }
}

/* ---- Parameters ---- */

void monk_synth_set_vowel(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_vowel(&s->voices[i], v);
}
void monk_synth_set_voice(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    s->base_voice = v;
    apply_voice_spread(s);
}
void monk_synth_set_glide(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_glide(&s->voices[i], v);
}
void monk_synth_set_vibrato(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_vibrato(&s->voices[i], v);
}
void monk_synth_set_vibrato_rate(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_vibrato_rate(&s->voices[i], v);
}
void monk_synth_set_aspiration(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_aspiration(&s->voices[i], v);
}
void monk_synth_set_attack(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_attack(&s->voices[i], v);
}
void monk_synth_set_decay(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_decay(&s->voices[i], v);
}
void monk_synth_set_sustain(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_sustain(&s->voices[i], v);
}
void monk_synth_set_release(MonkSynthEngine *s, float v) {
    if (!s)
        return;
    for (int i = 0; i < MAX_UNISON; i++)
        monk_voice_set_release(&s->voices[i], v);
}

void monk_synth_set_unison(MonkSynthEngine *s, int count) {
    if (!s)
        return;
    if (count < 1)
        count = 1;
    if (count > MAX_UNISON)
        count = MAX_UNISON;

    int old_count = s->unison_count;
    s->unison_count = count;
    s->target_voice_gain = 1.0f / sqrtf((float)count);

    if (old_count == count)
        return;

    /* If a note is sounding, adjust voices live */
    if (!monk_voice_is_active(&s->voices[0]))
        return;

    float base_hz = s->last_base_hz;

    /* Fade out excess voices — force into ENV_RELEASE directly so the
     * anti-click minimum (~3ms) applies even with default envelope. */
    for (int i = count; i < old_count; i++)
        s->voices[i].env_stage = ENV_RELEASE;

    /* Re-pitch existing voices to the new spread */
    for (int i = 0; i < count && i < old_count; i++)
        monk_voice_set_pitch_direct(&s->voices[i], detuned_hz(base_hz, s->unison_detune, i, count));

    /* Start new voices */
    for (int i = old_count; i < count; i++)
        monk_voice_note_on(&s->voices[i], detuned_hz(base_hz, s->unison_detune, i, count), 1.0f);

    apply_voice_spread(s);
}

void monk_synth_set_unison_detune(MonkSynthEngine *s, float cents) {
    if (!s)
        return;
    s->unison_detune = cents;

    /* Apply immediately if voices are sounding */
    if (monk_voice_is_active(&s->voices[0]) && s->unison_count > 1) {
        int n = s->unison_count;
        float base_hz = s->last_base_hz;
        for (int i = 0; i < n; i++)
            monk_voice_set_pitch_direct(&s->voices[i], detuned_hz(base_hz, cents, i, n));
    }
}

void monk_synth_set_unison_voice_spread(MonkSynthEngine *s, float spread) {
    if (!s)
        return;
    s->unison_voice_spread = spread;
    apply_voice_spread(s);
}

void monk_synth_set_delay_mix(MonkSynthEngine *s, float v) {
    if (s)
        monk_delay_set_mix(&s->delay, v);
}
void monk_synth_set_delay_rate(MonkSynthEngine *s, float v) {
    if (s)
        monk_delay_set_rate(&s->delay, v);
}
void monk_synth_set_volume(MonkSynthEngine *s, float v) {
    if (s)
        s->cc_volume = v;
}
void monk_synth_set_level(MonkSynthEngine *s, float v) {
    if (s)
        s->level = v;
}

/* ---- MIDI routing ---- */

/* Standard CC mapping for vocal synthesis parameters.
 * All values are 0.0–1.0 normalized (host/framework does the scaling). */
void monk_synth_midi_cc(MonkSynthEngine *s, uint8_t cc, float value) {
    if (!s)
        return;
    switch (cc) {
    case 1:
        monk_synth_set_vibrato(s, value);
        break;
    case 5:
        monk_synth_set_glide(s, value);
        break;
    case 7:
        s->cc_volume = value * 127.0f * 0.001f;
        break;
    case 12:
        monk_delay_set_mix(&s->delay, value);
        break;
    case 13:
        monk_synth_set_voice(s, value);
        break;
    default:
        break;
    }
}

void monk_synth_pitch_bend(MonkSynthEngine *s, float value) {
    if (!s)
        return;
    monk_synth_set_vowel(s, value);
}

/* ---- Audio processing ---- */

/* Process pipeline: sum unison voices → stereo delay → gain staging. */
void monk_synth_process(MonkSynthEngine *s, float *out_l, float *out_r, uint32_t num_samples) {
    if (!s || !out_l || !out_r)
        return;

    uint32_t n = num_samples < MAX_BUF ? num_samples : MAX_BUF;
    memset(s->scratch_mono, 0, n * sizeof(float));

    /* Sum all active voices into scratch_mono. We iterate ALL voices (not
     * just unison_count) so that releasing voices fade out properly when
     * the unison count is reduced. Gain is smoothed per-sample to prevent
     * clicks when the count changes mid-note. */
    for (int v = 0; v < MAX_UNISON; v++) {
        /* Always process voices in the current unison set (to drain overlap
         * buffers after note-off). Beyond that, only process releasing voices. */
        if (v >= s->unison_count && !monk_voice_is_active(&s->voices[v]))
            continue;
        memset(s->scratch_voice, 0, n * sizeof(float));
        monk_voice_process(&s->voices[v], s->scratch_voice, n);
        for (uint32_t i = 0; i < n; i++)
            s->scratch_mono[i] += s->scratch_voice[i];
    }

    /* Apply smoothed voice gain (1/sqrt(unison_count)) */
    float gain_step = (s->target_voice_gain - s->current_voice_gain) / (float)n;
    for (uint32_t i = 0; i < n; i++) {
        s->scratch_mono[i] *= s->current_voice_gain;
        s->current_voice_gain += gain_step;
    }
    s->current_voice_gain = s->target_voice_gain;

    monk_delay_process(&s->delay, s->scratch_mono, s->scratch_l, s->scratch_r, n);

    float gain = monk_voice_amplitude(&s->voices[0]) * s->cc_volume * s->level;
    for (uint32_t i = 0; i < n; i++) {
        out_l[i] = s->scratch_l[i] * gain;
        out_r[i] = s->scratch_r[i] * gain;
    }
}

/* ---- State query ---- */

int32_t monk_synth_is_active(const MonkSynthEngine *s) {
    if (!s)
        return 0;
    for (int i = 0; i < MAX_UNISON; i++)
        if (monk_voice_is_active(&s->voices[i]))
            return 1;
    return 0;
}

float monk_synth_amplitude(const MonkSynthEngine *s) {
    return s ? monk_voice_amplitude(&s->voices[0]) : 0.0f;
}

/* monk_midi_note_to_freq is defined in voice.c */
