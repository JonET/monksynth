#ifndef MONK_DELAY_H
#define MONK_DELAY_H

#include <stdint.h>

#define MONK_DELAY_LINE_SIZE 96000 /* ~2s at 48kHz, ~1s at 96kHz */

typedef struct {
    float buffer_l[MONK_DELAY_LINE_SIZE];
    float buffer_r[MONK_DELAY_LINE_SIZE];
    uint32_t write_pos;
    float target_delay_l; /* target delay in samples */
    float target_delay_r;
    float current_delay_l; /* smoothed delay in samples */
    float current_delay_r;
    float smooth_coeff; /* one-pole coefficient for delay smoothing */
    float sample_rate;
    float rate; /* 0-1 normalized, scales delay time */
    float feedback;
    float mix;
} MonkDelay;

void monk_delay_init(MonkDelay *d, float sample_rate);
void monk_delay_set_sample_rate(MonkDelay *d, float sample_rate);
void monk_delay_set_mix(MonkDelay *d, float mix);
void monk_delay_set_rate(MonkDelay *d, float rate);
void monk_delay_reset(MonkDelay *d);
void monk_delay_process(MonkDelay *d, const float *mono_in, float *out_l, float *out_r, uint32_t n);

#endif
