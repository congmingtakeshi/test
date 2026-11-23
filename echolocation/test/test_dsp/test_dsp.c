#include <unity.h>
#include "dsp.h"

void setUp(void) {}
void tearDown(void) {}

void test_stretch_length_is_n_in_times_factor(void) {
    int16_t in[4] = {0, 100, 200, 300};
    int16_t out[16] = {0};
    TEST_ASSERT_EQUAL(8, dsp_time_stretch(in, 4, 2.0f, out, 16));
}
void test_stretch_rejects_out_of_range_factor(void) {
    int16_t in[4] = {0}, out[64];
    TEST_ASSERT_EQUAL(0, dsp_time_stretch(in, 4, 0.5f, out, 64));
    TEST_ASSERT_EQUAL(0, dsp_time_stretch(in, 4, 21.0f, out, 64));
}
void test_stretch_truncates_at_out_cap(void) {
    int16_t in[100] = {0}, out[10];
    TEST_ASSERT_EQUAL(10, dsp_time_stretch(in, 100, 10.0f, out, 10));
}
void test_stretch_never_writes_past_out_cap(void) {
    int16_t in[100], out[12];
    for (int i = 0; i < 100; i++) in[i] = 1234;
    out[10] = 0x5a5a; out[11] = 0x5a5a;               // canaries
    dsp_time_stretch(in, 100, 10.0f, out, 10);
    TEST_ASSERT_EQUAL(0x5a5a, out[10]);
    TEST_ASSERT_EQUAL(0x5a5a, out[11]);
}
void test_stretch_interpolates_between_samples(void) {
    int16_t in[2] = {0, 100}, out[4];
    dsp_time_stretch(in, 2, 2.0f, out, 4);
    TEST_ASSERT_EQUAL(0, out[0]);
    TEST_ASSERT_EQUAL(50, out[1]);
    TEST_ASSERT_EQUAL(100, out[2]);
}
void test_blanking_zeroes_leading_samples_only(void) {
    int16_t b[10]; for (int i = 0; i < 10; i++) b[i] = 1000;
    dsp_apply_blanking(b, 10, 100000, 50);   // 100kHzで50us = 5サンプル
    TEST_ASSERT_EQUAL(0, b[4]);
    TEST_ASSERT_EQUAL(1000, b[5]);
}
void test_blanking_longer_than_buffer_does_not_overrun(void) {
    int16_t b[4] = {1, 1, 1, 1};
    dsp_apply_blanking(b, 4, 100000, 1000000);       // 100000サンプル分 > n
    TEST_ASSERT_EQUAL(0, b[3]);
}
void test_limiter_clips_both_polarities(void) {
    int16_t b[4] = {20000, -20000, 5000, -5000};
    dsp_limit_peak(b, 4, 8000);
    TEST_ASSERT_EQUAL(8000, b[0]); TEST_ASSERT_EQUAL(-8000, b[1]);
    TEST_ASSERT_EQUAL(5000, b[2]); TEST_ASSERT_EQUAL(-5000, b[3]);
}
void test_limiter_clips_int16_min(void) {
    int16_t b[1] = {INT16_MIN};
    dsp_limit_peak(b, 1, 8000);
    TEST_ASSERT_EQUAL(-8000, b[0]);
}
void test_interleave_order_is_l_then_r(void) {
    int16_t l[2] = {1, 2}, r[2] = {10, 20}, o[4];
    dsp_interleave_lr(l, r, 2, o);
    TEST_ASSERT_EQUAL(1, o[0]); TEST_ASSERT_EQUAL(10, o[1]);
    TEST_ASSERT_EQUAL(2, o[2]); TEST_ASSERT_EQUAL(20, o[3]);
}
void test_delay_to_distance_for_1m_wall(void) {
    uint32_t mm = dsp_delay_to_distance_mm(5831);   // 2m往復の時間
    TEST_ASSERT_UINT32_WITHIN(5, 1000, mm);
}
void test_step_factor_clamps_to_8_20(void) {
    TEST_ASSERT_EQUAL_FLOAT(8.0f, dsp_step_factor(8.0f, -1));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, dsp_step_factor(20.0f, +1));
    TEST_ASSERT_EQUAL_FLOAT(11.0f, dsp_step_factor(10.0f, +1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stretch_length_is_n_in_times_factor);
    RUN_TEST(test_stretch_rejects_out_of_range_factor);
    RUN_TEST(test_stretch_truncates_at_out_cap);
    RUN_TEST(test_stretch_never_writes_past_out_cap);
    RUN_TEST(test_stretch_interpolates_between_samples);
    RUN_TEST(test_blanking_zeroes_leading_samples_only);
    RUN_TEST(test_blanking_longer_than_buffer_does_not_overrun);
    RUN_TEST(test_limiter_clips_both_polarities);
    RUN_TEST(test_limiter_clips_int16_min);
    RUN_TEST(test_interleave_order_is_l_then_r);
    RUN_TEST(test_delay_to_distance_for_1m_wall);
    RUN_TEST(test_step_factor_clamps_to_8_20);
    return UNITY_END();
}
