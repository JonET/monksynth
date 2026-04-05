#include "delay.h"
#include <string.h>

/*
 * Stereo delay effect with feedback.
 *
 * Two independent delay lines at different tap times create a wide stereo
 * image. The left and right channels each feed back into themselves (no
 * cross-feedback). The mix parameter controls how much signal enters the
 * delay lines — at mix=0 the output is dry only, at mix=1 it's full wet.
 * Output is always dry + wet (not crossfaded).
 *
 * The rate parameter scales both tap times together: 0.5 = original timing,
 * 0 = very short slapback, 1 = long spacious echo.
 */

/* Base delay times in samples at 44100 Hz (~310ms left, ~398ms right) */
#define DELAY_TIME_L_44100 13653
#define DELAY_TIME_R_44100 17570

/* Recalculate target delay times (in samples). The actual read positions
 * are smoothly interpolated toward these targets in the process loop. */
static void recalc_taps(MonkDelay *d) {
    float scale = d->sample_rate / 44100.0f;
    float rate_scale = 0.1f + d->rate * 1.9f;

    float delay_l = DELAY_TIME_L_44100 * scale * rate_scale;
    float delay_r = DELAY_TIME_R_44100 * scale * rate_scale;

    if (delay_l >= MONK_DELAY_LINE_SIZE)
        delay_l = MONK_DELAY_LINE_SIZE - 1;
    if (delay_r >= MONK_DELAY_LINE_SIZE)
        delay_r = MONK_DELAY_LINE_SIZE - 1;

    d->target_delay_l = delay_l;
    d->target_delay_r = delay_r;
}

void monk_delay_init(MonkDelay *d, float sample_rate) {
    memset(d->buffer_l, 0, sizeof(d->buffer_l));
    memset(d->buffer_r, 0, sizeof(d->buffer_r));
    d->write_pos = 0;
    d->sample_rate = sample_rate;
    d->smooth_coeff = 1.0f / (0.05f * sample_rate); /* ~50ms smoothing */
    d->rate = 0.5f;
    d->feedback = 0.5f;
    d->mix = 0.5f;
    recalc_taps(d);
    d->current_delay_l = d->target_delay_l;
    d->current_delay_r = d->target_delay_r;
}

void monk_delay_set_sample_rate(MonkDelay *d, float sample_rate) {
    d->sample_rate = sample_rate;
    d->smooth_coeff = 1.0f / (0.05f * sample_rate);
    memset(d->buffer_l, 0, sizeof(d->buffer_l));
    memset(d->buffer_r, 0, sizeof(d->buffer_r));
    d->write_pos = 0;
    recalc_taps(d);
    d->current_delay_l = d->target_delay_l;
    d->current_delay_r = d->target_delay_r;
}

void monk_delay_set_mix(MonkDelay *d, float mix) {
    if (mix < 0.0f)
        mix = 0.0f;
    if (mix > 1.0f)
        mix = 1.0f;
    d->mix = mix;
}

void monk_delay_set_rate(MonkDelay *d, float rate) {
    if (rate < 0.0f)
        rate = 0.0f;
    if (rate > 1.0f)
        rate = 1.0f;
    d->rate = rate;
    recalc_taps(d);
}

void monk_delay_reset(MonkDelay *d) {
    memset(d->buffer_l, 0, sizeof(d->buffer_l));
    memset(d->buffer_r, 0, sizeof(d->buffer_r));
}

void monk_delay_process(MonkDelay *d, const float *mono_in, float *out_l, float *out_r,
                        uint32_t n) {
    float coeff = d->smooth_coeff;

    for (uint32_t i = 0; i < n; i++) {
        float input = mono_in[i];

        /* Smooth delay time toward target */
        d->current_delay_l += coeff * (d->target_delay_l - d->current_delay_l);
        d->current_delay_r += coeff * (d->target_delay_r - d->current_delay_r);

        /* Read with linear interpolation for fractional positions */
        float rd_l = (float)d->write_pos - d->current_delay_l;
        if (rd_l < 0.0f)
            rd_l += MONK_DELAY_LINE_SIZE;
        int idx_l = (int)rd_l;
        float frac_l = rd_l - idx_l;
        float tap_l = d->buffer_l[idx_l] * (1.0f - frac_l) +
                      d->buffer_l[(idx_l + 1) % MONK_DELAY_LINE_SIZE] * frac_l;

        float rd_r = (float)d->write_pos - d->current_delay_r;
        if (rd_r < 0.0f)
            rd_r += MONK_DELAY_LINE_SIZE;
        int idx_r = (int)rd_r;
        float frac_r = rd_r - idx_r;
        float tap_r = d->buffer_r[idx_r] * (1.0f - frac_r) +
                      d->buffer_r[(idx_r + 1) % MONK_DELAY_LINE_SIZE] * frac_r;

        d->buffer_l[d->write_pos] = (tap_l * d->feedback + input) * d->mix;
        d->buffer_r[d->write_pos] = (tap_r * d->feedback + input) * d->mix;

        out_l[i] = input + tap_l;
        out_r[i] = input + tap_r;

        d->write_pos = (d->write_pos + 1) % MONK_DELAY_LINE_SIZE;
    }
}
