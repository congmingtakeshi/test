#include "dsp.h"

#define SPEED_OF_SOUND_MM_PER_MS 343u  // 343 m/s

size_t dsp_time_stretch(const int16_t *in, size_t n_in, float factor,
                        int16_t *out, size_t out_cap)
{
    if (!in || !out || n_in == 0 || factor < 1.0f || factor > 20.0f) return 0;
    size_t n_out = (size_t)((float)n_in * factor);
    if (n_out > out_cap) n_out = out_cap;
    for (size_t i = 0; i < n_out; i++) {
        float pos = (float)i / factor;
        size_t idx = (size_t)pos;
        if (idx >= n_in) idx = n_in - 1;
        float frac = pos - (float)idx;
        float a = in[idx];
        float b = (idx + 1 < n_in) ? in[idx + 1] : a;
        out[i] = (int16_t)(a + (b - a) * frac);
    }
    return n_out;
}

void dsp_apply_blanking(int16_t *buf, size_t n, uint32_t fs_hz, uint32_t blank_us)
{
    uint64_t count = (uint64_t)fs_hz * blank_us / 1000000u;
    if (count > n) count = n;
    for (size_t i = 0; i < (size_t)count; i++) buf[i] = 0;
}

void dsp_limit_peak(int16_t *buf, size_t n, int16_t ceiling)
{
    for (size_t i = 0; i < n; i++) {
        if (buf[i] > ceiling) buf[i] = ceiling;
        else if (buf[i] < -ceiling) buf[i] = (int16_t)-ceiling;
    }
}

void dsp_interleave_lr(const int16_t *l, const int16_t *r, size_t n, int16_t *out)
{
    for (size_t i = 0; i < n; i++) {
        out[2 * i] = l[i];
        out[2 * i + 1] = r[i];
    }
}

uint32_t dsp_delay_to_distance_mm(uint32_t delay_us)
{
    // 343 mm/ms = 0.343 mm/us; halve for one way.
    return (uint32_t)((uint64_t)delay_us * SPEED_OF_SOUND_MM_PER_MS / 2000u);
}

float dsp_step_factor(float current, int dir)
{
    float next = current + (dir > 0 ? 1.0f : dir < 0 ? -1.0f : 0.0f);
    if (next < DSP_FACTOR_MIN) next = DSP_FACTOR_MIN;
    if (next > DSP_FACTOR_MAX) next = DSP_FACTOR_MAX;
    return next;
}
