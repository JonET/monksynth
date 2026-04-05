#ifndef MONK_VOICE_H
#define MONK_VOICE_H

#include <stdbool.h>
#include <stdint.h>

#define MONK_OVERLAP_BUF_SIZE 10240
#define MONK_NUM_FORMANTS 3
#define MONK_SPLINE_TBL_SIZE 1280
#define MONK_SINE_TBL_SIZE 1024
#define MONK_MAX_GRAIN 3840 /* 192000 * 0.02 — max at 192kHz */

typedef struct {
    float sample_rate;

    bool active;
    float current_pitch; /* MIDI note space */
    float target_pitch;

    float glide_param;
    float min_glide; /* minimum glide for XY pad smoothing, independent of knob */

    float vibrato_phase;
    float vibrato_depth;
    float vibrato_rate; /* 0-1 normalized, scales LFO speed */
    float random_jitter;
    uint32_t jitter_counter;
    uint32_t jitter_period;
    float sr_per_vib_tbl;
    uint32_t rng_state;

    float overlap_buf[MONK_OVERLAP_BUF_SIZE];
    uint32_t overlap_write_pos;
    uint32_t overlap_read_pos;
    uint32_t overlap_offset;

    float grain[MONK_MAX_GRAIN];
    uint32_t grain_len;
    float current_vowel;
    float current_voice;
    float aspiration_amp;
    bool grain_dirty;

    /* ADSR envelope */
    enum { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE } env_stage;
    float env_level;   /* current envelope value 0-1 */
    float env_attack;  /* attack time in seconds (0 = instant) */
    float env_decay;   /* decay time in seconds (0 = instant) */
    float env_sustain; /* sustain level 0-1 (1 = full) */
    float env_release; /* release time in seconds (0 = instant) */

    float sine_tbl[MONK_SINE_TBL_SIZE];
    float formant_tbl[MONK_NUM_FORMANTS][MONK_SPLINE_TBL_SIZE];
    float cos_window[MONK_MAX_GRAIN];
    float exp_decay[MONK_MAX_GRAIN * 4];
    float aspiration[MONK_MAX_GRAIN];
} MonkVoice;

void monk_voice_init(MonkVoice *v, float sample_rate);
void monk_voice_set_sample_rate(MonkVoice *v, float sample_rate);
void monk_voice_reset(MonkVoice *v);
void monk_voice_note_on(MonkVoice *v, float pitch_hz, float velocity);
void monk_voice_note_off(MonkVoice *v);
bool monk_voice_is_active(const MonkVoice *v);
void monk_voice_set_pitch_direct(MonkVoice *v, float hz);
void monk_voice_set_pitch_target(MonkVoice *v, float hz);
void monk_voice_set_vowel(MonkVoice *v, float vowel);
void monk_voice_set_voice(MonkVoice *v, float voice);
void monk_voice_set_glide(MonkVoice *v, float glide);
void monk_voice_set_vibrato(MonkVoice *v, float depth);
void monk_voice_set_vibrato_rate(MonkVoice *v, float rate);
void monk_voice_set_aspiration(MonkVoice *v, float amp);
void monk_voice_set_attack(MonkVoice *v, float seconds);
void monk_voice_set_decay(MonkVoice *v, float seconds);
void monk_voice_set_sustain(MonkVoice *v, float level);
void monk_voice_set_release(MonkVoice *v, float seconds);
float monk_voice_amplitude(const MonkVoice *v);
void monk_voice_process(MonkVoice *v, float *output, uint32_t n);

float monk_note_to_hz(float note);
float monk_hz_to_note(float hz);
float monk_midi_note_to_freq(uint8_t note);

#endif
