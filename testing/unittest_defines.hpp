#ifndef _NAIVE_RTC_UNITTEST_DEFINES_BASE_H_
#define _NAIVE_RTC_UNITTEST_DEFINES_BASE_H_

#if ENABLE_UNIT_TESTS
#define T(x)            x
#define MY_TEST(x, y)   TEST(x, y)
#define MY_TEST_P(x, y) TEST_P(x, y)
#define MY_TEST_F(x, y) TEST_F(x, y)
#define MY_INSTANTIATE_TEST_SUITE_P(x, y, ...) INSTANTIATE_TEST_SUITE_P(x, y, __VA_ARGS__)
#else
#define T(x)            FILTERED_##x
#define MY_TEST(x, y)   TEST(FILTERED_##x, y)
#define MY_TEST_P(x, y) TEST_P(FILTERED_##x, y)
#define MY_TEST_F(x, y) TEST_F(FILTERED_##x, y)
#define MY_INSTANTIATE_TEST_SUITE_P(x, y, ...) INSTANTIATE_TEST_SUITE_P(FILTERED_##x, FILTERED_##y, __VA_ARGS__)
#endif

#endif