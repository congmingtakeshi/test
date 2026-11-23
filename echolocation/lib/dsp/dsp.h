// Hardware-independent DSP helpers for the echolocation headset.
#ifndef DSP_H
#define DSP_H

#include <stddef.h>
#include <stdint.h>

#define DSP_FACTOR_MIN 8.0f    // adjustable stretch range (spec: 8-20x)
#define DSP_FACTOR_MAX 20.0f

// Stretches `in` by `factor` (linear interpolation). Returns samples written,
// never more than out_cap. Returns 0 if factor < 1.0 or factor > 20.0.
size_t dsp_time_stretch(const int16_t *in, size_t n_in, float factor,
                        int16_t *out, size_t out_cap);

// Zeroes the first blank_us microseconds of buf (clamped to n samples).
void dsp_apply_blanking(int16_t *buf, size_t n, uint32_t fs_hz, uint32_t blank_us);

// Clamps |sample| to ceiling (positive).
void dsp_limit_peak(int16_t *buf, size_t n, int16_t ceiling);

// out = L,R,L,R,... (2*n samples).
void dsp_interleave_lr(const int16_t *l, const int16_t *r, size_t n, int16_t *out);

// One-way distance in mm from a round-trip delay, speed of sound 343 m/s.
uint32_t dsp_delay_to_distance_mm(uint32_t delay_us);

// current +1/-1 step (0 = unchanged), clamped to [8.0, 20.0].
float dsp_step_factor(float current, int dir);

#endif
