#include "rtc/base/units/time_delta.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(TimeDeltaBaseTest, ConstExpr) {
    constexpr int64_t kValue = -12345;
    constexpr TimeDelta kTestUnitZero = TimeDelta::Zero();
    constexpr TimeDelta kTestUnitMaxValue = TimeDelta::MaxValue();
    constexpr TimeDelta kTestUnitMinValue = TimeDelta::MinValue();

    static_assert(kTestUnitZero.IsZero(), "");
    static_assert(kTestUnitMaxValue.IsMax(), "");
    static_assert(kTestUnitMinValue.IsMin(), "");
    static_assert(kTestUnitMaxValue.ms_or(-1) == -1, "");

    static_assert(kTestUnitMaxValue > kTestUnitZero, "");

    constexpr TimeDelta kTimeDeltaSeconds = TimeDelta::Seconds(kValue);
    constexpr TimeDelta kTimeDeltaMs = TimeDelta::Millis(kValue);
    constexpr TimeDelta kTimeDeltaUs = TimeDelta::Micros(kValue);

    static_assert(kTimeDeltaSeconds.seconds_or(0) == kValue, "");
    static_assert(kTimeDeltaMs.ms_or(0) == kValue, "");
    static_assert(kTimeDeltaUs.us_or(0) == kValue, "");
}

TEST(TimeDeltaBaseTest, GetBackSameValues) {
    const int64_t kValue = 499;
    for (int sign = -1; sign <= 1; ++sign) {
        int64_t value = kValue * sign;
        EXPECT_EQ(TimeDelta::Millis(value).ms(), value);
        EXPECT_EQ(TimeDelta::Micros(value).us(), value);
        EXPECT_EQ(TimeDelta::Seconds(value).seconds(), value);
        EXPECT_EQ(TimeDelta::Seconds(value).seconds(), value);
    }
    EXPECT_EQ(TimeDelta::Zero().us(), 0);
}

TEST(TimeDeltaBaseTest, GetDifferentPrefix) {
    const int64_t kValue = 3000000;
    EXPECT_EQ(TimeDelta::Micros(kValue).seconds(), kValue / 1000000);
    EXPECT_EQ(TimeDelta::Millis(kValue).seconds(), kValue / 1000);
    EXPECT_EQ(TimeDelta::Micros(kValue).ms(), kValue / 1000);

    EXPECT_EQ(TimeDelta::Millis(kValue).us(), kValue * 1000);
    EXPECT_EQ(TimeDelta::Seconds(kValue).ms(), kValue * 1000);
    EXPECT_EQ(TimeDelta::Seconds(kValue).us(), kValue * 1000000);
}

TEST(TimeDeltaBaseTest, ConvertsToAndFromDouble) {
    const int64_t kMicros = 17017;
    const double kNanosDouble = kMicros * 1e3;
    const double kMicrosDouble = kMicros;
    const double kMillisDouble = kMicros * 1e-3;
    const double kSecondsDouble = kMillisDouble * 1e-3;

    EXPECT_EQ(TimeDelta::Micros(kMicros).seconds<double>(), kSecondsDouble);
    EXPECT_EQ(TimeDelta::Seconds(kSecondsDouble).us(), kMicros);

    EXPECT_EQ(TimeDelta::Micros(kMicros).ms<double>(), kMillisDouble);
    EXPECT_EQ(TimeDelta::Millis(kMillisDouble).us(), kMicros);

    EXPECT_EQ(TimeDelta::Micros(kMicros).us<double>(), kMicrosDouble);
    EXPECT_EQ(TimeDelta::Micros(kMicrosDouble).us(), kMicros);

    EXPECT_NEAR(TimeDelta::Micros(kMicros).ns<double>(), kNanosDouble, 1);

    const double kMaxValue = std::numeric_limits<double>::infinity();
    const double kMinValue = -kMaxValue;

    EXPECT_EQ(TimeDelta::MaxValue().seconds<double>(), kMaxValue);
    EXPECT_EQ(TimeDelta::MinValue().seconds<double>(), kMinValue);
    EXPECT_EQ(TimeDelta::MaxValue().ms<double>(), kMaxValue);
    EXPECT_EQ(TimeDelta::MinValue().ms<double>(), kMinValue);
    EXPECT_EQ(TimeDelta::MaxValue().us<double>(), kMaxValue);
    EXPECT_EQ(TimeDelta::MinValue().us<double>(), kMinValue);
    EXPECT_EQ(TimeDelta::MaxValue().ns<double>(), kMaxValue);
    EXPECT_EQ(TimeDelta::MinValue().ns<double>(), kMinValue);

    EXPECT_TRUE(TimeDelta::Seconds(kMaxValue).IsMax());
    EXPECT_TRUE(TimeDelta::Seconds(kMinValue).IsMin());
    EXPECT_TRUE(TimeDelta::Millis(kMaxValue).IsMax());
    EXPECT_TRUE(TimeDelta::Millis(kMinValue).IsMin());
    EXPECT_TRUE(TimeDelta::Micros(kMaxValue).IsMax());
    EXPECT_TRUE(TimeDelta::Micros(kMinValue).IsMin());
}

TEST(TimeDeltaBaseTest, InfinityOperations) {
  const int64_t kValue = 267;
  const TimeDelta finite = TimeDelta::Millis(kValue);
  EXPECT_TRUE((TimeDelta::MaxValue() + finite).IsMax());
  EXPECT_TRUE((TimeDelta::MaxValue() - finite).IsMax());
  EXPECT_TRUE((finite + TimeDelta::MaxValue()).IsMax());
  EXPECT_TRUE((finite - TimeDelta::MinValue()).IsMax());

  EXPECT_TRUE((TimeDelta::MinValue() + finite).IsMin());
  EXPECT_TRUE((TimeDelta::MinValue() - finite).IsMin());
  EXPECT_TRUE((finite + TimeDelta::MinValue()).IsMin());
  EXPECT_TRUE((finite - TimeDelta::MaxValue()).IsMin());
}
    
} // namespace test    
} // namespace naivertc
