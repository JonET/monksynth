#include "voice.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f
#define TAU 6.28318530717958647692f

/*
 * FOF (Formant-Wave-Function) vocal synthesis.
 *
 * The voice produces a stream of short "grains" — windowed bursts of three
 * damped sinusoids at formant frequencies plus breathy aspiration noise.
 * Grains overlap-add into a circular buffer at a rate determined by the
 * fundamental pitch. The vowel sound comes from interpolating formant
 * frequencies across five vowel positions [a, e, i, o, u] using cubic
 * splines. Algorithm attributed to Xavier Rodet (IRCAM).
 */

#define SPLINE_SEG_SIZE 320        /* samples per cubic spline segment (4 segments total) */
#define PITCH_TABLE_BASE 8.175799f /* frequency of MIDI note 0 (C-1) */
#define GRAIN_DURATION 0.02f       /* grain length in seconds (882 samples at 44100 Hz) */
#define ASPIRATION_AMP_DEFAULT 0.5f
#define ASPIRATION_PERIOD1 0.000202f /* period factor for ~4951 Hz breath component */
#define ASPIRATION_PERIOD2 0.000263f /* period factor for ~3802 Hz breath component */
#define ENV_ANTICLICK 0.003f         /* minimum ramp to prevent clicks (~3ms) */

/* Formant center frequencies (Hz) at five vowel positions [a, e, i, o, u] */
static const float FORMANT_FREQS[MONK_NUM_FORMANTS][5] = {
    {280.0f, 450.0f, 800.0f, 350.0f, 270.0f},      /* F1 */
    {600.0f, 800.0f, 1150.0f, 2000.0f, 2140.0f},   /* F2 */
    {2240.0f, 2830.0f, 2900.0f, 2800.0f, 2950.0f}, /* F3 */
};

/* Formant bandwidths — control exponential decay rate per formant */
static const float FORMANT_BW[MONK_NUM_FORMANTS] = {32.5f, 47.5f, 62.5f};

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

/* ---- Table builders ---- */

static void build_sine_table(MonkVoice *v) {
    for (int i = 0; i < MONK_SINE_TBL_SIZE; i++)
        v->sine_tbl[i] = sinf(TAU * (float)i / MONK_SINE_TBL_SIZE);
}

/* Catmull-Rom spline interpolation across the five vowel control points.
 * Endpoints are duplicated so the curve passes through all five positions.
 * This gives a smooth frequency sweep as the vowel parameter moves 0→1. */
static void build_formant_tables(MonkVoice *v) {
    for (int f = 0; f < MONK_NUM_FORMANTS; f++) {
        const float *pts = FORMANT_FREQS[f];
        float padded[7] = {pts[0], pts[0], pts[1], pts[2], pts[3], pts[4], pts[4]};

        for (int seg = 0; seg < 4; seg++) {
            float p0 = padded[seg], p1 = padded[seg + 1];
            float p2 = padded[seg + 2], p3 = padded[seg + 3];

            for (int i = 0; i < SPLINE_SEG_SIZE; i++) {
                float t = (float)i / SPLINE_SEG_SIZE;
                float t2 = t * t, t3 = t2 * t;
                v->formant_tbl[f][seg * SPLINE_SEG_SIZE + i] =
                    0.5f *
                    ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                     (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
            }
        }
    }
}

/* Grain envelope: short cosine attack, flat sustain, cosine release.
 * Not a full Hann window — the release doesn't reach zero, giving a
 * slightly "open" tail. The exponential decay table is 4x grain length
 * to accommodate different formant bandwidths indexing into it. */
static void build_window_and_decay(MonkVoice *v) {
    uint32_t n = v->grain_len;
    float sr = v->sample_rate;

    uint32_t attack_len = (uint32_t)(sr * 0.0018f);
    uint32_t release_start = (uint32_t)(sr * 0.013f);
    uint32_t release_len = (uint32_t)(sr * 0.007f);

    for (uint32_t i = 0; i < n; i++)
        v->cos_window[i] = 1.0f;

    for (uint32_t i = 0; i < attack_len && i < n; i++)
        v->cos_window[i] = 0.5f * (1.0f - cosf(PI * (float)i / (float)attack_len));

    for (uint32_t i = release_start; i < n; i++)
        v->cos_window[i] = 0.5f * (1.0f - cosf(PI * (float)(release_len + i) / (float)release_len));

    float bw = 50.0f * PI;
    for (uint32_t i = 0; i < n * 4; i++)
        v->exp_decay[i] = expf(-bw * (float)i / sr);
}

/* Aspiration noise: two inharmonic sinusoids with exponential decay.
 * Creates the breathy "air" quality of a singing voice. The frequencies
 * (~4951 Hz and ~3802 Hz) are deliberately non-harmonic to sound noisy. */
static void build_aspiration(MonkVoice *v) {
    uint32_t n = v->grain_len;
    float sr = v->sample_rate;
    float p1 = sr * ASPIRATION_PERIOD1;
    float p2 = sr * ASPIRATION_PERIOD2;
    uint32_t decay_len = v->grain_len * 4;

    for (uint32_t i = 0; i < n; i++) {
        float s1 = sinf(TAU * (float)i / p1);
        uint32_t d1_idx = (uint32_t)((float)i * 3.0f);
        float d1 = d1_idx < decay_len ? v->exp_decay[d1_idx] : 0.0f;

        float s2 = sinf(TAU * (float)i / p2);
        uint32_t d2_idx = (uint32_t)((float)i * 3.5f);
        float d2 = d2_idx < decay_len ? v->exp_decay[d2_idx] : 0.0f;

        v->aspiration[i] = s1 * d1 + s2 * d2;
    }
}

/* ---- Internal helpers ---- */

/* LCG pseudorandom number generator. Returns [0, 1). Deterministic
 * sequence — same seed always produces the same vibrato jitter pattern. */
static float next_rand(MonkVoice *v) {
    v->rng_state = v->rng_state * 1103515245u + 12345u;
    return (float)((v->rng_state >> 16) & 0x7FFF) / 32768.0f;
}

/* Advance the ADSR envelope by one sample. Returns the current level. */
static float env_tick(MonkVoice *v) {
    float rate;
    switch (v->env_stage) {
    case ENV_ATTACK:
        rate = 1.0f / (fmaxf(v->env_attack, ENV_ANTICLICK) * v->sample_rate);
        v->env_level += rate;
        if (v->env_level >= 1.0f) {
            v->env_level = 1.0f;
            v->env_stage = v->env_decay > 0.0f ? ENV_DECAY : ENV_SUSTAIN;
        }
        break;
    case ENV_DECAY:
        rate = v->env_decay > 0.0f ? 1.0f / (v->env_decay * v->sample_rate) : 1.0f;
        v->env_level -= rate * (1.0f - v->env_sustain);
        if (v->env_level <= v->env_sustain) {
            v->env_level = v->env_sustain;
            v->env_stage = ENV_SUSTAIN;
        }
        break;
    case ENV_SUSTAIN:
        break;
    case ENV_RELEASE:
        rate = 1.0f / (fmaxf(v->env_release, ENV_ANTICLICK) * v->sample_rate);
        v->env_level -= rate;
        if (v->env_level <= 0.0f) {
            v->env_level = 0.0f;
            v->env_stage = ENV_IDLE;
            v->active = false;
        }
        break;
    case ENV_IDLE:
    default:
        break;
    }
    return v->env_level;
}

/* Linear interpolation into the precomputed formant spline table. */
static float lookup_formant(const MonkVoice *v, int formant, float vowel) {
    float pos = clampf(vowel, 0.0f, 1.0f) * (MONK_SPLINE_TBL_SIZE - 1);
    int idx = (int)pos;
    if (idx > MONK_SPLINE_TBL_SIZE - 2)
        idx = MONK_SPLINE_TBL_SIZE - 2;
    float frac = pos - (float)idx;
    return v->formant_tbl[formant][idx] * (1.0f - frac) + v->formant_tbl[formant][idx + 1] * frac;
}

/* Pitch-to-grain-period conversion with quantization.
 * Quantizes to 32 steps per semitone, then converts the table index back
 * to a frequency. The quantization is
 * audible and intentional — it gives the voice its characteristic stepped
 * pitch quality. */
static float grain_period(const MonkVoice *v, float midi_note) {
    float internal = midi_note - 12.0f;
    int table_idx = (int)(internal * 32.0f);
    float freq = powf(2.0f, (float)table_idx / 384.0f) * PITCH_TABLE_BASE;
    return v->sample_rate / freq;
}

/* Two portamento modes matching the original Delay Lama:
 *
 * XY pad (min_glide > 0): 10-tick decimated linear ramp. Each tick
 * fires every ~10ms. When a new target arrives, step = delta/10.
 * Continuous mouse input resets the ramp, creating natural ease-out.
 *
 * MIDI notes (min_glide = 0): per-sample constant-rate slew at
 * ±12 semitones/sec scaled by glide_param. Snaps within 0.2 semitones.
 * At default glide (0.5), one octave takes ~510ms. */
#define RAMP_TICKS 10
static void apply_portamento(MonkVoice *v) {
    if (v->min_glide > 0.001f) {
        /* XY pad: tick-based linear ramp */
        v->ramp_counter--;
        if (v->ramp_counter > 0)
            return;
        v->ramp_counter = v->ramp_period;

        if (v->pitch_ramp_ticks > 0) {
            v->pitch_ramp_ticks--;
            v->current_pitch += v->pitch_step;
            if (v->pitch_ramp_ticks == 0)
                v->current_pitch = v->target_pitch;
        }
        if (v->vowel_ramp_ticks > 0) {
            v->vowel_ramp_ticks--;
            v->current_vowel += v->vowel_step;
            if (v->vowel_ramp_ticks == 0)
                v->current_vowel = v->target_vowel;
        }
    } else if (v->glide_param > 0.001f) {
        /* MIDI: per-sample constant-rate slew */
        float diff = v->target_pitch - v->current_pitch;
        float rate = 12.0f / ((v->glide_param + 0.01f) * v->sample_rate);
        if (diff > 0.2f)
            v->current_pitch += rate;
        else if (diff < -0.2f)
            v->current_pitch -= rate;
        else
            v->current_pitch = v->target_pitch;
        v->current_vowel = v->target_vowel;
    } else {
        /* No glide: snap */
        v->current_pitch = v->target_pitch;
        v->current_vowel = v->target_vowel;
    }
}

/* Sine LFO vibrato with random jitter on the rate. Returns a semitone
 * offset to add to the current pitch. Even at depth=0, a subtle 0.2-semitone
 * wobble keeps the voice sounding alive. The jitter randomizes the LFO
 * rate periodically so vibrato doesn't sound mechanical. */
static float compute_vibrato(MonkVoice *v) {
    /* Rate parameter scales the LFO speed: 0.5 = default,
     * 0 = ~1/4 speed, 1 = ~4x speed. Exponential mapping for musical feel. */
    float rate_scale = powf(4.0f, v->vibrato_rate * 2.0f - 1.0f);
    float rate = (v->vibrato_depth * 0.2f + 1.0f) * v->random_jitter * rate_scale;
    v->vibrato_phase += rate / v->sr_per_vib_tbl;

    while (v->vibrato_phase >= (float)MONK_SINE_TBL_SIZE)
        v->vibrato_phase -= (float)MONK_SINE_TBL_SIZE;

    v->jitter_counter++;
    if (v->jitter_counter >= v->jitter_period) {
        v->jitter_counter = 0;
        v->random_jitter = next_rand(v) * 2.0f + 5.0f;
    }

    int idx = (int)v->vibrato_phase % MONK_SINE_TBL_SIZE;
    return (v->vibrato_depth + 0.2f) * v->sine_tbl[idx];
}

/* Build one grain: sum three damped formant sinusoids plus aspiration noise,
 * shaped by the cosine window envelope. Each formant is a sine wave at
 * the interpolated vowel frequency, multiplied by an exponential decay
 * controlled by that formant's bandwidth. The voice parameter scales all
 * formant frequencies (0.75 = baritone, 1.0 = normal, 1.25 = soprano). */
static void compute_grain(MonkVoice *v) {
    uint32_t n = v->grain_len;
    float formant_scale = v->current_voice * 0.5f + 0.75f;
    float sr = v->sample_rate;

    float formants[MONK_NUM_FORMANTS];
    for (int f = 0; f < MONK_NUM_FORMANTS; f++)
        formants[f] = lookup_formant(v, f, v->current_vowel) * formant_scale;

    uint32_t decay_len = n * 4;

    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / sr;
        float sample = 0.0f;

        for (int f = 0; f < MONK_NUM_FORMANTS; f++) {
            float phase = TAU * formants[f] * t;
            uint32_t sine_idx = (uint32_t)(phase / TAU * MONK_SINE_TBL_SIZE) % MONK_SINE_TBL_SIZE;
            float sine = v->sine_tbl[sine_idx];

            uint32_t decay_idx = (uint32_t)(FORMANT_BW[f] * (float)i / 50.0f);
            float decay = decay_idx < decay_len ? v->exp_decay[decay_idx] : 0.0f;

            sample += sine * decay;
        }

        sample += v->aspiration[i] * v->aspiration_amp;
        v->grain[i] = sample * v->cos_window[i];
    }

    v->grain_dirty = false;
}

/* Accumulate the grain waveform into the circular overlap buffer.
 * The write position advances by the number of samples since the last
 * grain trigger, so grains overlap when the pitch period is shorter
 * than the grain length (which is most of the time). */
static void overlap_add(MonkVoice *v) {
    v->overlap_write_pos = (v->overlap_write_pos + v->overlap_offset) % MONK_OVERLAP_BUF_SIZE;
    uint32_t start = v->overlap_write_pos;

    for (uint32_t i = 0; i < v->grain_len; i++) {
        uint32_t pos = (start + i) % MONK_OVERLAP_BUF_SIZE;
        v->overlap_buf[pos] += v->grain[i];
    }
}

/* ---- Public API ---- */

void monk_voice_init(MonkVoice *v, float sample_rate) {
    memset(v, 0, sizeof(*v));
    v->sample_rate = sample_rate;
    v->current_pitch = monk_hz_to_note(220.0f);
    v->target_pitch = monk_hz_to_note(220.0f);
    v->random_jitter = 4.0f;
    v->jitter_period = 4586;
    v->sr_per_vib_tbl = sample_rate / (float)MONK_SINE_TBL_SIZE;
    v->rng_state = 12345;
    v->grain_len = (uint32_t)(sample_rate * GRAIN_DURATION);
    v->ramp_period = (int)(sample_rate * 0.01f); /* ~10ms between ticks */
    v->ramp_counter = v->ramp_period;
    v->current_vowel = 0.5f;
    v->target_vowel = 0.5f;
    v->current_voice = 0.5f;
    v->vibrato_rate = 0.5f;
    v->aspiration_amp = ASPIRATION_AMP_DEFAULT;
    v->grain_dirty = true;

    /* ADSR defaults: instant on/off */
    v->env_stage = ENV_IDLE;
    v->env_level = 0.0f;
    v->env_attack = 0.0f;
    v->env_decay = 0.0f;
    v->env_sustain = 1.0f;
    v->env_release = 0.0f;

    build_sine_table(v);
    build_formant_tables(v);
    build_window_and_decay(v);
    build_aspiration(v);
}

void monk_voice_set_sample_rate(MonkVoice *v, float sample_rate) {
    v->sample_rate = sample_rate;
    v->sr_per_vib_tbl = sample_rate / (float)MONK_SINE_TBL_SIZE;
    uint32_t jp = (uint32_t)(sample_rate / 44100.0f * 4586.0f);
    v->jitter_period = jp > 0 ? jp : 1;
    v->grain_len = (uint32_t)(sample_rate * GRAIN_DURATION);

    build_window_and_decay(v);
    build_aspiration(v);
    v->grain_dirty = true;
}

void monk_voice_reset(MonkVoice *v) {
    v->active = false;
    v->current_pitch = monk_hz_to_note(220.0f);
    v->target_pitch = monk_hz_to_note(220.0f);
    v->vibrato_phase = 0.0f;
    v->random_jitter = 4.0f;
    v->jitter_counter = 0;
    v->rng_state = 12345;
    memset(v->overlap_buf, 0, sizeof(v->overlap_buf));
    v->overlap_write_pos = 0;
    v->overlap_read_pos = 0;
    v->overlap_offset = 0;
    v->grain_dirty = true;
}

void monk_voice_note_on(MonkVoice *v, float pitch_hz, float velocity) {
    (void)velocity;
    bool was_active = v->active;
    v->active = true;
    v->target_pitch = monk_hz_to_note(pitch_hz);
    if (!was_active) {
        v->current_pitch = v->target_pitch;
        v->grain_dirty = true;
        /* Don't clear the overlap buffer — it drains naturally (each sample
         * is zeroed after being read). Clearing it on retrigger causes a pop
         * when notes are back-to-back because residual grain data is abruptly
         * zeroed. Sync write pos to read pos so new grains land correctly. */
        v->overlap_write_pos = v->overlap_read_pos;
        v->overlap_offset = 0;
    }
    bool has_env = v->env_attack > 0.0f || v->env_decay > 0.0f || v->env_sustain < 1.0f ||
                   v->env_release > 0.0f;
    if (has_env) {
        v->env_stage = ENV_ATTACK;
        /* Don't reset env_level — ramp from current position to avoid
         * clicks when re-triggering during a release tail. The anti-click
         * minimum in env_tick ensures a smooth ~3ms ramp even with attack=0. */
    } else {
        v->env_level = 1.0f;
        v->env_stage = ENV_SUSTAIN;
    }
}

void monk_voice_note_off(MonkVoice *v) {
    bool has_env = v->env_attack > 0.0f || v->env_decay > 0.0f || v->env_sustain < 1.0f ||
                   v->env_release > 0.0f;
    if (has_env) {
        /* Always ramp down — env_tick applies a 3ms minimum even when
         * release=0, preventing clicks from hard-cutting mid-grain. */
        v->env_stage = ENV_RELEASE;
    } else {
        v->active = false;
        v->env_stage = ENV_IDLE;
        v->env_level = 0.0f;
    }
}

/* Voice is "active" during release too — it's still producing sound. */
bool monk_voice_is_active(const MonkVoice *v) { return v->active || v->env_stage == ENV_RELEASE; }

void monk_voice_set_pitch_direct(MonkVoice *v, float hz) {
    float note = monk_hz_to_note(hz);
    v->target_pitch = note;
    v->current_pitch = note;
}

/* Set target pitch and reset the linear ramp. The glide knob scales
 * the tick count; min_glide (XY pad) always uses 10 ticks. */
void monk_voice_set_pitch_target(MonkVoice *v, float hz) {
    float new_target = monk_hz_to_note(hz);
    float eff = v->glide_param > v->min_glide ? v->glide_param : v->min_glide;
    if (eff < 0.001f) {
        v->current_pitch = new_target;
        v->pitch_ramp_ticks = 0;
    } else {
        int n = RAMP_TICKS;
        if (eff > 0.5f)
            n = RAMP_TICKS + (int)((eff - 0.5f) * 180.0f);
        v->pitch_step = (new_target - v->current_pitch) / (float)n;
        v->pitch_ramp_ticks = n;
    }
    v->target_pitch = new_target;
}

void monk_voice_set_vowel(MonkVoice *v, float vowel) {
    float new_target = clampf(vowel, 0.0f, 1.0f);
    float eff = v->glide_param > v->min_glide ? v->glide_param : v->min_glide;
    if (eff < 0.001f) {
        v->current_vowel = new_target;
        v->vowel_ramp_ticks = 0;
    } else {
        int n = RAMP_TICKS;
        if (eff > 0.5f)
            n = RAMP_TICKS + (int)((eff - 0.5f) * 180.0f);
        v->vowel_step = (new_target - v->current_vowel) / (float)n;
        v->vowel_ramp_ticks = n;
    }
    v->target_vowel = new_target;
}

void monk_voice_set_voice(MonkVoice *v, float voice) {
    v->current_voice = clampf(voice, 0.0f, 1.0f);
}

void monk_voice_set_glide(MonkVoice *v, float glide) { v->glide_param = glide; }

void monk_voice_set_vibrato(MonkVoice *v, float depth) {
    v->vibrato_depth = clampf(depth, 0.0f, 1.0f);
}

void monk_voice_set_vibrato_rate(MonkVoice *v, float rate) {
    v->vibrato_rate = clampf(rate, 0.0f, 1.0f);
}

void monk_voice_set_aspiration(MonkVoice *v, float amp) {
    v->aspiration_amp = clampf(amp, 0.0f, 1.0f);
}

void monk_voice_set_attack(MonkVoice *v, float seconds) { v->env_attack = seconds; }
void monk_voice_set_decay(MonkVoice *v, float seconds) { v->env_decay = seconds; }
void monk_voice_set_sustain(MonkVoice *v, float level) {
    v->env_sustain = clampf(level, 0.0f, 1.0f);
}
void monk_voice_set_release(MonkVoice *v, float seconds) { v->env_release = seconds; }

/* Pitch-dependent amplitude compensation. Higher pitches produce shorter
 * grain periods with fewer overlapping grains, so they'd be quieter without
 * this correction. Linear ramp: louder at low pitches, quieter at high. */
float monk_voice_amplitude(const MonkVoice *v) {
    float internal = v->current_pitch - 12.0f;
    return clampf(internal * (-1.0f / 72.0f) + 2.0f, 0.1f, 3.0f);
}

void monk_voice_process(MonkVoice *v, float *output, uint32_t n) {
    bool has_envelope = v->env_attack > 0.0f || v->env_decay > 0.0f || v->env_sustain < 1.0f ||
                        v->env_release > 0.0f || v->env_stage == ENV_RELEASE;

    /* Pass 1: synthesis — trigger grains into the overlap buffer.
     * Runs while voice is active OR during envelope release. */
    for (uint32_t i = 0; i < n; i++) {
        if (v->active || v->env_stage == ENV_RELEASE) {
            apply_portamento(v);

            float vib = compute_vibrato(v);
            float pitch = v->current_pitch + vib;
            uint32_t period = (uint32_t)grain_period(v, pitch);

            if (v->overlap_offset >= period || v->grain_dirty) {
                compute_grain(v);
                overlap_add(v);
                v->overlap_offset = 0;
            }
        }
        v->overlap_offset++;
    }

    /* Pass 2: read from the overlap buffer, apply envelope */
    while (v->overlap_read_pos >= MONK_OVERLAP_BUF_SIZE)
        v->overlap_read_pos -= MONK_OVERLAP_BUF_SIZE;

    for (uint32_t i = 0; i < n; i++) {
        float sample = v->overlap_buf[v->overlap_read_pos];
        v->overlap_buf[v->overlap_read_pos] = 0.0f;
        v->overlap_read_pos = (v->overlap_read_pos + 1) % MONK_OVERLAP_BUF_SIZE;

        if (has_envelope)
            sample *= env_tick(v);

        output[i] = sample;
    }
}

/* ---- Pitch utilities ---- */

float monk_note_to_hz(float note) { return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f); }

float monk_hz_to_note(float hz) { return 12.0f * log2f(hz / 440.0f) + 69.0f; }

float monk_midi_note_to_freq(uint8_t note) { return monk_note_to_hz((float)note); }
