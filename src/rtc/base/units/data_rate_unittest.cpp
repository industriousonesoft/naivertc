#include "rtc/base/units/data_rate.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(DataRateTest, ConstExpr) {
    constexpr int64_t kValue = 12345;
    constexpr DataRate kDataRateZero = DataRate::Zero();
    constexpr DataRate kDataRateInf = DataRate::Infinity();
    static_assert(kDataRateZero.IsZero(), "");
    static_assert(kDataRateInf.IsInfinite(), "");
    static_assert(kDataRateInf.bps_or(-1) == -1, "");
    static_assert(kDataRateInf > kDataRateZero, "");

    constexpr DataRate kDataRateBps = DataRate::BitsPerSec(kValue);
    constexpr DataRate kDataRateKbps = DataRate::KilobitsPerSec(kValue);
    static_assert(kDataRateBps.bps<double>() == kValue, "");
    static_assert(kDataRateBps.bps_or(0) == kValue, "");
    static_assert(kDataRateKbps.kbps_or(0) == kValue, "");
}

MY_TEST(DataRateTest, GetBackSameValues) {
    const int64_t kValue = 123 * 8;
    EXPECT_EQ(DataRate::BitsPerSec(kValue).bps(), kValue);
    EXPECT_EQ(DataRate::KilobitsPerSec(kValue).kbps(), kValue);
}

MY_TEST(DataRateTest, GetDifferentPrefix) {
    const int64_t kValue = 123 * 8000;
    EXPECT_EQ(DataRate::BitsPerSec(kValue).kbps(), kValue / 1000);
}

MY_TEST(DataRateTest, IdentityChecks) {
    const int64_t kValue = 3000;
    EXPECT_TRUE(DataRate::Zero().IsZero());
    EXPECT_FALSE(DataRate::BitsPerSec(kValue).IsZero());

    EXPECT_TRUE(DataRate::Infinity().IsInfinite());
    EXPECT_FALSE(DataRate::Zero().IsInfinite());
    EXPECT_FALSE(DataRate::BitsPerSec(kValue).IsInfinite());

    EXPECT_FALSE(DataRate::Infinity().IsFinite());
    EXPECT_TRUE(DataRate::BitsPerSec(kValue).IsFinite());
    EXPECT_TRUE(DataRate::Zero().IsFinite());
}

MY_TEST(DataRateTest, ComparisonOperators) {
    const int64_t kSmall = 450;
    const int64_t kLarge = 451;
    const DataRate small = DataRate::BitsPerSec(kSmall);
    const DataRate large = DataRate::BitsPerSec(kLarge);

    EXPECT_EQ(DataRate::Zero(), DataRate::BitsPerSec(0));
    EXPECT_EQ(DataRate::Infinity(), DataRate::Infinity());
    EXPECT_EQ(small, small);
    EXPECT_LE(small, small);
    EXPECT_GE(small, small);
    EXPECT_NE(small, large);
    EXPECT_LE(small, large);
    EXPECT_LT(small, large);
    EXPECT_GE(large, small);
    EXPECT_GT(large, small);
    EXPECT_LT(DataRate::Zero(), small);
    EXPECT_GT(DataRate::Infinity(), large);
}

MY_TEST(DataRateTest, ConvertsToAndFromDouble) {
    const int64_t kValue = 128;
    const double kDoubleValue = static_cast<double>(kValue);
    const double kDoubleKbps = kValue * 1e-3;
    const double kFloatKbps = static_cast<float>(kDoubleKbps);

    EXPECT_EQ(DataRate::BitsPerSec(kValue).bps<double>(), kDoubleValue);
    EXPECT_EQ(DataRate::BitsPerSec(kValue).kbps<double>(), kDoubleKbps);
    EXPECT_EQ(DataRate::BitsPerSec(kValue).kbps<float>(), kFloatKbps);
    EXPECT_EQ(DataRate::BitsPerSec(kDoubleValue).bps(), kValue);
    EXPECT_EQ(DataRate::KilobitsPerSec(kDoubleKbps).bps(), kValue);

    const double kInfinity = std::numeric_limits<double>::infinity();
    EXPECT_EQ(DataRate::Infinity().bps<double>(), kInfinity);
    EXPECT_TRUE(DataRate::BitsPerSec(kInfinity).IsInfinite());
    EXPECT_TRUE(DataRate::KilobitsPerSec(kInfinity).IsInfinite());
}

MY_TEST(DataRateTest, Clamping) {
    const DataRate upper = DataRate::KilobitsPerSec(800);
    const DataRate lower = DataRate::KilobitsPerSec(100);
    const DataRate under = DataRate::KilobitsPerSec(100);
    const DataRate inside = DataRate::KilobitsPerSec(500);
    const DataRate over = DataRate::KilobitsPerSec(1000);
    EXPECT_EQ(under.Clamped(lower, upper), lower);
    EXPECT_EQ(inside.Clamped(lower, upper), inside);
    EXPECT_EQ(over.Clamped(lower, upper), upper);

    DataRate mutable_rate = lower;
    mutable_rate.Clamp(lower, upper);
    EXPECT_EQ(mutable_rate, lower);
    mutable_rate = inside;
    mutable_rate.Clamp(lower, upper);
    EXPECT_EQ(mutable_rate, inside);
    mutable_rate = over;
    mutable_rate.Clamp(lower, upper);
    EXPECT_EQ(mutable_rate, upper);
}
    
} // namespace test
} // namespace naivertc
