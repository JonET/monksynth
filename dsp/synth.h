/*
 * MonkSynthEngine — public C API.
 *
 * MonkSynthEngine manages the vocal synthesizer, stereo delay, monophonic note
 * stack, and MIDI CC routing. All calls must be on the same thread.
 */

#ifndef MONK_SYNTH_H
#define MONK_SYNTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MonkSynthEngine MonkSynthEngine;

/* Lifecycle */
MonkSynthEngine *monk_synth_new(float sample_rate);
void monk_synth_free(MonkSynthEngine *s);
void monk_synth_set_sample_rate(MonkSynthEngine *s, float sample_rate);
void monk_synth_reset(MonkSynthEngine *s);

/* Note control */
void monk_synth_note_on(MonkSynthEngine *s, uint8_t note, float velocity);
void monk_synth_note_off(MonkSynthEngine *s, uint8_t note);
void monk_synth_set_pitch_hz(MonkSynthEngine *s, float hz);
void monk_synth_restore_note_stack(MonkSynthEngine *s);

/* Parameters (0.0–1.0) */
void monk_synth_set_vowel(MonkSynthEngine *s, float value);
void monk_synth_set_voice(MonkSynthEngine *s, float value);
void monk_synth_set_glide(MonkSynthEngine *s, float value);
void monk_synth_set_vibrato(MonkSynthEngine *s, float value);
void monk_synth_set_vibrato_rate(MonkSynthEngine *s, float value);
void monk_synth_set_pitch_bend(MonkSynthEngine *s, float semitones);
void monk_synth_set_aspiration(MonkSynthEngine *s, float value);
void monk_synth_set_attack(MonkSynthEngine *s, float seconds);
void monk_synth_set_decay(MonkSynthEngine *s, float seconds);
void monk_synth_set_sustain(MonkSynthEngine *s, float level);
void monk_synth_set_release(MonkSynthEngine *s, float seconds);
void monk_synth_set_unison(MonkSynthEngine *s, int count);
void monk_synth_set_unison_detune(MonkSynthEngine *s, float cents);
void monk_synth_set_unison_voice_spread(MonkSynthEngine *s, float spread);
void monk_synth_set_delay_mix(MonkSynthEngine *s, float value);
void monk_synth_set_delay_rate(MonkSynthEngine *s, float value);
void monk_synth_set_volume(MonkSynthEngine *s, float value);
void monk_synth_set_level(MonkSynthEngine *s, float value);

/* MIDI routing */
void monk_synth_midi_cc(MonkSynthEngine *s, uint8_t cc, float value);

/* State readback (for UI animation) */
float monk_synth_get_vowel(MonkSynthEngine *s);
float monk_synth_get_pitch_normalized(MonkSynthEngine *s);

/* Audio processing */
void monk_synth_process(MonkSynthEngine *s, float *out_l, float *out_r, uint32_t num_samples);

/* State query */
int32_t monk_synth_is_active(const MonkSynthEngine *s);
float monk_synth_amplitude(const MonkSynthEngine *s);

/* Utility */
float monk_midi_note_to_freq(uint8_t note);

#ifdef __cplusplus
}
#endif

#endif
