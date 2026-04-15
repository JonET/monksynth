/*
 * Synth-level tests for MonkSynthEngine.
 * Uses synth_internal.h to peek at the held-note stack and unison voices.
 */

#include "synth.h"
#include "synth_internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define SR 44100.0f

static int float_near(float a, float b, float tol) {
    float d = a - b;
    if (d < 0.0f)
        d = -d;
    return d <= tol;
}

static void test_note_stack_lifo(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_note_on(s, 64, 1.0f);
    monk_synth_note_on(s, 67, 1.0f);

    assert(s->held_count == 3);
    assert(s->held[0].note == 60);
    assert(s->held[1].note == 64);
    assert(s->held[2].note == 67);

    /* Release the newest note: should fall back to 64 */
    monk_synth_note_off(s, 67);
    assert(s->held_count == 2);
    assert(s->held[s->held_count - 1].note == 64);

    /* Release the middle note: stack still has 60; top is 60 */
    monk_synth_note_off(s, 64);
    assert(s->held_count == 1);
    assert(s->held[s->held_count - 1].note == 60);

    monk_synth_free(s);
}

static void test_note_stack_overflow(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    /* Push 20 notes into a 16-slot stack */
    for (uint8_t n = 50; n < 70; n++)
        monk_synth_note_on(s, n, 1.0f);

    /* The stack caps at 16; the first 16 notes are kept, later pushes drop. */
    assert(s->held_count == MONK_MAX_NOTES);
    assert(s->held[0].note == 50);
    assert(s->held[MONK_MAX_NOTES - 1].note == 65);

    monk_synth_free(s);
}

static void test_unison_detune_propagates(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_set_unison(s, 3);
    monk_synth_set_unison_detune(s, 50.0f); /* ±50 cents */
    monk_synth_note_on(s, 69, 1.0f);        /* A4 */

    assert(s->unison_count == 3);

    /* Voice 0 should be detuned -50 cents, voice 2 +50 cents, voice 1 center.
     * 50 cents = 0.5 semitones in MIDI note space. */
    float center = s->voices[1].target_pitch;
    float v0 = s->voices[0].target_pitch;
    float v2 = s->voices[2].target_pitch;

    assert(float_near(center, 69.0f, 0.01f));
    assert(float_near(v0, 69.0f - 0.5f, 0.01f));
    assert(float_near(v2, 69.0f + 0.5f, 0.01f));

    monk_synth_free(s);
}

static void test_pitch_bend_propagates_to_all_voices(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_set_unison(s, 5);
    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_set_pitch_bend(s, 7.0f);

    for (int i = 0; i < 5; i++)
        assert(float_near(s->voices[i].pitch_bend_offset, 7.0f, 1e-6f));

    monk_synth_set_pitch_bend(s, -3.5f);
    for (int i = 0; i < 5; i++)
        assert(float_near(s->voices[i].pitch_bend_offset, -3.5f, 1e-6f));

    monk_synth_free(s);
}

static void test_reset_clears_notes(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_note_on(s, 64, 1.0f);
    assert(s->held_count == 2);

    monk_synth_reset(s);
    assert(s->held_count == 0);

    monk_synth_free(s);
}

int main(void) {
    test_note_stack_lifo();
    test_note_stack_overflow();
    test_unison_detune_propagates();
    test_pitch_bend_propagates_to_all_voices();
    test_reset_clears_notes();

    printf("test_synth: all tests passed\n");
    return 0;
}
