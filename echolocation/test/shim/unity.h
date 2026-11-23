// Minimal stand-in for ThrowTheSwitch Unity, used only when the real Unity
// cannot be downloaded (see ledger ruling). Macro names match real Unity so
// test files run unchanged under `pio test -e native`.
#ifndef UNITY_SHIM_H
#define UNITY_SHIM_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int unity_fail_count = 0;
static int unity_test_count = 0;
static int unity_current_failed = 0;

#define UNITY_FAIL_MSG(file, line, msg) \
    do { printf("%s:%d: FAIL: %s\n", file, line, msg); unity_current_failed = 1; } while (0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { long long e_ = (long long)(expected), a_ = (long long)(actual); \
         if (e_ != a_) { char m_[96]; snprintf(m_, sizeof m_, "expected %lld was %lld", e_, a_); \
                         UNITY_FAIL_MSG(__FILE__, __LINE__, m_); return; } } while (0)

#define TEST_ASSERT_UINT32_WITHIN(delta, expected, actual) \
    do { long long e_ = (long long)(expected), a_ = (long long)(actual), d_ = (long long)(delta); \
         if (a_ < e_ - d_ || a_ > e_ + d_) { char m_[96]; \
             snprintf(m_, sizeof m_, "expected %lld +/- %lld was %lld", e_, d_, a_); \
             UNITY_FAIL_MSG(__FILE__, __LINE__, m_); return; } } while (0)

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual) \
    do { float e_ = (expected), a_ = (actual); \
         if (fabsf(e_ - a_) > 1e-4f) { char m_[96]; \
             snprintf(m_, sizeof m_, "expected %f was %f", e_, a_); \
             UNITY_FAIL_MSG(__FILE__, __LINE__, m_); return; } } while (0)

#define TEST_ASSERT_TRUE(cond) \
    do { if (!(cond)) { UNITY_FAIL_MSG(__FILE__, __LINE__, "condition false"); return; } } while (0)

void setUp(void);
void tearDown(void);

#define UNITY_BEGIN() do { unity_fail_count = 0; unity_test_count = 0; } while (0)
#define RUN_TEST(fn) \
    do { unity_current_failed = 0; setUp(); fn(); tearDown(); unity_test_count++; \
         if (unity_current_failed) { unity_fail_count++; printf("%s: FAIL\n", #fn); } \
         else printf("%s: PASS\n", #fn); } while (0)
#define UNITY_END() (printf("%d Tests %d Failures\n", unity_test_count, unity_fail_count), \
                     unity_fail_count ? 1 : 0)

#endif
