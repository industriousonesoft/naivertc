#include "rtc/base/units/bit_rate.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(BitRateTest, ConstExpr) {
    constexpr int64_t kValue = 12345;
    constexpr BitRate kBitRateZero = BitRate::Zero();
    constexpr BitRate kBitRateInf = BitRate::Infinity();
    static_assert(kBitRateZero.IsZero(), "");
    static_assert(kBitRateInf.IsInfinite(), "");
    static_assert(kBitRateInf.bps_or(-1) == -1, "");
    static_assert(kBitRateInf > kBitRateZero, "");

    constexpr BitRate kBitRateBps = BitRate::BitsPerSec(kValue);
    constexpr BitRate kBitRateKbps = BitRate::KilobitsPerSec(kValue);
    static_assert(kBitRateBps.bps<double>() == kValue, "");
    static_assert(kBitRateBps.bps_or(0) == kValue, "");
    static_assert(kBitRateKbps.kbps_or(0) == kValue, "");
}

TEST(BitRateTest, GetBackSameValues) {
    const int64_t kValue = 123 * 8;
    EXPECT_EQ(BitRate::BitsPerSec(kValue).bps(), kValue);
    EXPECT_EQ(BitRate::KilobitsPerSec(kValue).kbps(), kValue);
}

TEST(BitRateTest, GetDifferentPrefix) {
    const int64_t kValue = 123 * 8000;
    EXPECT_EQ(BitRate::BitsPerSec(kValue).kbps(), kValue / 1000);
}

TEST(BitRateTest, IdentityChecks) {
    const int64_t kValue = 3000;
    EXPECT_TRUE(BitRate::Zero().IsZero());
    EXPECT_FALSE(BitRate::BitsPerSec(kValue).IsZero());

    EXPECT_TRUE(BitRate::Infinity().IsInfinite());
    EXPECT_FALSE(BitRate::Zero().IsInfinite());
    EXPECT_FALSE(BitRate::BitsPerSec(kValue).IsInfinite());

    EXPECT_FALSE(BitRate::Infinity().IsFinite());
    EXPECT_TRUE(BitRate::BitsPerSec(kValue).IsFinite());
    EXPECT_TRUE(BitRate::Zero().IsFinite());
}

TEST(BitRateTest, ComparisonOperators) {
    const int64_t kSmall = 450;
    const int64_t kLarge = 451;
    const BitRate small = BitRate::BitsPerSec(kSmall);
    const BitRate large = BitRate::BitsPerSec(kLarge);

    EXPECT_EQ(BitRate::Zero(), BitRate::BitsPerSec(0));
    EXPECT_EQ(BitRate::Infinity(), BitRate::Infinity());
    EXPECT_EQ(small, small);
    EXPECT_LE(small, small);
    EXPECT_GE(small, small);
    EXPECT_NE(small, large);
    EXPECT_LE(small, large);
    EXPECT_LT(small, large);
    EXPECT_GE(large, small);
    EXPECT_GT(large, small);
    EXPECT_LT(BitRate::Zero(), small);
    EXPECT_GT(BitRate::Infinity(), large);
}

TEST(BitRateTest, ConvertsToAndFromDouble) {
    const int64_t kValue = 128;
    const double kDoubleValue = static_cast<double>(kValue);
    const double kDoubleKbps = kValue * 1e-3;
    const double kFloatKbps = static_cast<float>(kDoubleKbps);

    EXPECT_EQ(BitRate::BitsPerSec(kValue).bps<double>(), kDoubleValue);
    EXPECT_EQ(BitRate::BitsPerSec(kValue).kbps<double>(), kDoubleKbps);
    EXPECT_EQ(BitRate::BitsPerSec(kValue).kbps<float>(), kFloatKbps);
    EXPECT_EQ(BitRate::BitsPerSec(kDoubleValue).bps(), kValue);
    EXPECT_EQ(BitRate::KilobitsPerSec(kDoubleKbps).bps(), kValue);

    const double kInfinity = std::numeric_limits<double>::infinity();
    EXPECT_EQ(BitRate::Infinity().bps<double>(), kInfinity);
    EXPECT_TRUE(BitRate::BitsPerSec(kInfinity).IsInfinite());
    EXPECT_TRUE(BitRate::KilobitsPerSec(kInfinity).IsInfinite());
}

TEST(BitRateTest, Clamping) {
    const BitRate upper = BitRate::KilobitsPerSec(800);
    const BitRate lower = BitRate::KilobitsPerSec(100);
    const BitRate under = BitRate::KilobitsPerSec(100);
    const BitRate inside = BitRate::KilobitsPerSec(500);
    const BitRate over = BitRate::KilobitsPerSec(1000);
    EXPECT_EQ(under.Clamped(lower, upper), lower);
    EXPECT_EQ(inside.Clamped(lower, upper), inside);
    EXPECT_EQ(over.Clamped(lower, upper), upper);

    BitRate mutable_rate = lower;
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
