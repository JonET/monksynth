/*
 * Stereo delay tests for MonkDelay.
 * The delay struct has large buffers (~768 KB total), so we heap-allocate.
 */

#include "delay.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 44100.0f

static int float_near(float a, float b, float tol) {
    float d = a - b;
    if (d < 0.0f)
        d = -d;
    return d <= tol;
}

static void test_silence_in_silence_out(void) {
    MonkDelay *d = calloc(1, sizeof(MonkDelay));
    assert(d);
    monk_delay_init(d, SR);
    monk_delay_set_mix(d, 0.0f);
    d->feedback = 0.0f;

    float in[1024];
    float outL[1024];
    float outR[1024];
    memset(in, 0, sizeof(in));

    monk_delay_process(d, in, outL, outR, 1024);

    for (int i = 0; i < 1024; i++) {
        assert(float_near(outL[i], 0.0f, 1e-6f));
        assert(float_near(outR[i], 0.0f, 1e-6f));
    }
    free(d);
}

static void test_dry_passthrough_when_mix_is_zero(void) {
    MonkDelay *d = calloc(1, sizeof(MonkDelay));
    assert(d);
    monk_delay_init(d, SR);
    monk_delay_set_mix(d, 0.0f);
    d->feedback = 0.0f;

    float in[64];
    float outL[64];
    float outR[64];
    for (int i = 0; i < 64; i++)
        in[i] = (i == 0) ? 1.0f : 0.0f; /* impulse */

    monk_delay_process(d, in, outL, outR, 64);

    /* With mix=0, nothing enters the delay line, but the process function
     * still adds the dry signal to the output. Impulse must pass through
     * in the first sample. */
    assert(float_near(outL[0], 1.0f, 1e-6f));
    assert(float_near(outR[0], 1.0f, 1e-6f));
    for (int i = 1; i < 64; i++) {
        assert(float_near(outL[i], 0.0f, 1e-6f));
        assert(float_near(outR[i], 0.0f, 1e-6f));
    }
    free(d);
}

static void test_impulse_appears_at_tap_time(void) {
    MonkDelay *d = calloc(1, sizeof(MonkDelay));
    assert(d);
    monk_delay_init(d, SR);
    monk_delay_set_mix(d, 1.0f);
    monk_delay_set_rate(d, 0.5f); /* default rate */
    d->feedback = 0.0f;

    /* Feed an impulse and run for enough samples to hear both taps. The
     * default left tap at 44100 Hz is DELAY_TIME_L_44100 (13653) samples
     * scaled by rate_scale; at rate=0.5 that's 13653 * 1.0 = 13653. */
    int n = 20000;
    float *in = calloc(n, sizeof(float));
    float *outL = calloc(n, sizeof(float));
    float *outR = calloc(n, sizeof(float));
    assert(in && outL && outR);

    in[0] = 1.0f;
    monk_delay_process(d, in, outL, outR, n);

    /* Find the peak in the region where the left tap should appear. */
    float peak = 0.0f;
    int peak_idx = -1;
    for (int i = 1000; i < n; i++) {
        if (outL[i] > peak) {
            peak = outL[i];
            peak_idx = i;
        }
    }

    /* The peak should be somewhere between 10000 and 16000 samples (giving
     * the smoother time to settle) — the exact position moves with the
     * smoothing filter, so we only assert the approximate region. */
    assert(peak > 0.5f);
    assert(peak_idx > 10000 && peak_idx < 16000);

    free(in);
    free(outL);
    free(outR);
    free(d);
}

static void test_feedback_stability(void) {
    MonkDelay *d = calloc(1, sizeof(MonkDelay));
    assert(d);
    monk_delay_init(d, SR);
    monk_delay_set_mix(d, 1.0f);
    d->feedback = 0.9f;

    int n = 88200; /* 2 seconds */
    float *in = calloc(n, sizeof(float));
    float *outL = calloc(n, sizeof(float));
    float *outR = calloc(n, sizeof(float));
    assert(in && outL && outR);
    in[0] = 1.0f;

    monk_delay_process(d, in, outL, outR, n);

    /* With feedback 0.9, a single impulse should produce a decaying train
     * of echoes bounded well under 10.0. If anything blows up (NaN, Inf,
     * or overflow) the delay line is broken. */
    for (int i = 0; i < n; i++) {
        assert(!isnan(outL[i]) && !isnan(outR[i]));
        assert(!isinf(outL[i]) && !isinf(outR[i]));
        assert(outL[i] < 10.0f && outL[i] > -10.0f);
        assert(outR[i] < 10.0f && outR[i] > -10.0f);
    }

    free(in);
    free(outL);
    free(outR);
    free(d);
}

static void test_rate_setter_clamps(void) {
    MonkDelay *d = calloc(1, sizeof(MonkDelay));
    assert(d);
    monk_delay_init(d, SR);

    monk_delay_set_rate(d, 2.0f);
    assert(d->rate == 1.0f);
    monk_delay_set_rate(d, -1.0f);
    assert(d->rate == 0.0f);
    monk_delay_set_mix(d, 2.0f);
    assert(d->mix == 1.0f);
    monk_delay_set_mix(d, -1.0f);
    assert(d->mix == 0.0f);

    free(d);
}

int main(void) {
    test_silence_in_silence_out();
    test_dry_passthrough_when_mix_is_zero();
    test_impulse_appears_at_tap_time();
    test_feedback_stability();
    test_rate_setter_clamps();

    printf("test_delay: all tests passed\n");
    return 0;
}
