#include "rtc/base/units/timestamp.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(Base_TimestampTest, ConstExpr) {
    constexpr int64_t kValue = 12345;
    constexpr Timestamp kMaxValue = Timestamp::MaxValue();
    static_assert(kMaxValue.IsInfinite(), "");
    static_assert(kMaxValue.ms_or(-1) == -1, "");

    constexpr Timestamp kTimestampSeconds = Timestamp::Seconds(kValue);
    constexpr Timestamp kTimestampMs = Timestamp::Millis(kValue);
    constexpr Timestamp kTimestampUs = Timestamp::Micros(kValue);

    static_assert(kTimestampSeconds.seconds_or(0) == kValue, "");
    static_assert(kTimestampMs.ms_or(0) == kValue, "");
    static_assert(kTimestampUs.us_or(0) == kValue, "");

    static_assert(kTimestampMs > kTimestampUs, "");

    EXPECT_EQ(kTimestampSeconds.seconds(), kValue);
    EXPECT_EQ(kTimestampMs.ms(), kValue);
    EXPECT_EQ(kTimestampUs.us(), kValue);
}

TEST(Base_TimestampTest, GetBackSameValues) {
    const int64_t kValue = 499;
    EXPECT_EQ(Timestamp::Millis(kValue).ms(), kValue);
    EXPECT_EQ(Timestamp::Micros(kValue).us(), kValue);
    EXPECT_EQ(Timestamp::Seconds(kValue).seconds(), kValue);
}

TEST(Base_TimestampTest, GetDifferentPrefix) {
    const int64_t kValue = 3000000;
    EXPECT_EQ(Timestamp::Micros(kValue).seconds(), kValue / 1000000);
    EXPECT_EQ(Timestamp::Millis(kValue).seconds(), kValue / 1000);
    EXPECT_EQ(Timestamp::Micros(kValue).ms(), kValue / 1000);

    EXPECT_EQ(Timestamp::Millis(kValue).us(), kValue * 1000);
    EXPECT_EQ(Timestamp::Seconds(kValue).ms(), kValue * 1000);
    EXPECT_EQ(Timestamp::Seconds(kValue).us(), kValue * 1000000);
}


TEST(Base_TimestampTest, IdentityChecks) {
    const int64_t kValue = 3000;

    EXPECT_TRUE(Timestamp::MaxValue().IsInfinite());
    EXPECT_TRUE(Timestamp::MinValue().IsInfinite());
    EXPECT_FALSE(Timestamp::Millis(kValue).IsInfinite());

    EXPECT_FALSE(Timestamp::MaxValue().IsFinite());
    EXPECT_FALSE(Timestamp::MinValue().IsFinite());
    EXPECT_TRUE(Timestamp::Millis(kValue).IsFinite());

    EXPECT_TRUE(Timestamp::MaxValue().IsMax());
    EXPECT_FALSE(Timestamp::MinValue().IsMax());

    EXPECT_TRUE(Timestamp::MinValue().IsMin());
    EXPECT_FALSE(Timestamp::MaxValue().IsMin());
}

TEST(Base_TimestampTest, ConvertsToAndFromDouble) {
    const int64_t kMicros = 17017;
    const double kMicrosDouble = kMicros;
    const double kMillisDouble = kMicros * 1e-3;
    const double kSecondsDouble = kMillisDouble * 1e-3;

    EXPECT_EQ(Timestamp::Micros(kMicros).seconds<double>(), kSecondsDouble);
    EXPECT_EQ(Timestamp::Seconds(kSecondsDouble).us(), kMicros);

    EXPECT_EQ(Timestamp::Micros(kMicros).ms<double>(), kMillisDouble);
    EXPECT_EQ(Timestamp::Millis(kMillisDouble).us(), kMicros);

    EXPECT_EQ(Timestamp::Micros(kMicros).us<double>(), kMicrosDouble);
    EXPECT_EQ(Timestamp::Micros(kMicrosDouble).us(), kMicros);

    const double kMaxValue = std::numeric_limits<double>::infinity();
    const double kMinValue = -kMaxValue;

    EXPECT_EQ(Timestamp::MaxValue().seconds<double>(), kMaxValue);
    EXPECT_EQ(Timestamp::MinValue().seconds<double>(), kMinValue);
    EXPECT_EQ(Timestamp::MaxValue().ms<double>(), kMaxValue);
    EXPECT_EQ(Timestamp::MinValue().ms<double>(), kMinValue);
    EXPECT_EQ(Timestamp::MaxValue().us<double>(), kMaxValue);
    EXPECT_EQ(Timestamp::MinValue().us<double>(), kMinValue);

    EXPECT_TRUE(Timestamp::Seconds(kMaxValue).IsMax());
    EXPECT_TRUE(Timestamp::Seconds(kMinValue).IsMin());
    EXPECT_TRUE(Timestamp::Millis(kMaxValue).IsMax());
    EXPECT_TRUE(Timestamp::Millis(kMinValue).IsMin());
    EXPECT_TRUE(Timestamp::Micros(kMaxValue).IsMax());
    EXPECT_TRUE(Timestamp::Micros(kMinValue).IsMin());
}

TEST(Base_UnitConversionTest, TimestampAndTimeDeltaMath) {
    const int64_t kValueA = 267;
    const int64_t kValueB = 450;
    const Timestamp time_a = Timestamp::Millis(kValueA);
    const Timestamp time_b = Timestamp::Millis(kValueB);
    const TimeDelta delta_a = TimeDelta::Millis(kValueA);
    const TimeDelta delta_b = TimeDelta::Millis(kValueB);

    EXPECT_EQ((time_a - time_b), TimeDelta::Millis(kValueA - kValueB));
    EXPECT_EQ((time_b - delta_a), Timestamp::Millis(kValueB - kValueA));
    EXPECT_EQ((time_b + delta_a), Timestamp::Millis(kValueB + kValueA));

    Timestamp mutable_time = time_a;
    mutable_time += delta_b;
    EXPECT_EQ(mutable_time, time_a + delta_b);
    mutable_time -= delta_b;
    EXPECT_EQ(mutable_time, time_a);
}

TEST(Base_UnitConversionTest, InfinityOperations) {
    const int64_t kValue = 267;
    const Timestamp finite_time = Timestamp::Millis(kValue);
    const TimeDelta finite_delta = TimeDelta::Millis(kValue);
    EXPECT_TRUE((Timestamp::MaxValue() + finite_delta).IsInfinite());
    EXPECT_TRUE((Timestamp::MaxValue() - finite_delta).IsInfinite());
    EXPECT_TRUE((finite_time + TimeDelta::MaxValue()).IsInfinite());
    EXPECT_TRUE((finite_time - TimeDelta::MinValue()).IsInfinite());
}
    
} // namespace test
} // namespace naivertc 