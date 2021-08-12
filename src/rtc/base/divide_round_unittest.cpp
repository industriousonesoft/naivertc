#include "rtc/base/divide_round.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace naivertc {
namespace test {

// DivideRoundUp unittests
TEST(DivideRoundUpTest, CanBeUsedAsConstexpr) {
    static_assert(DivideRoundUp(5, 1) == 5, "");
    static_assert(DivideRoundUp(5, 2) == 3, "");
}

TEST(DivideRoundUpTest, ReturnsZeroForZeroDividend) {
    EXPECT_EQ(DivideRoundUp(uint8_t{0}, 1), 0);
    EXPECT_EQ(DivideRoundUp(uint8_t{0}, 3), 0);
    EXPECT_EQ(DivideRoundUp(int{0}, 1), 0);
    EXPECT_EQ(DivideRoundUp(int{0}, 3), 0);
}

TEST(DivideRoundUpTest, WorksForMaxDividend) {
    EXPECT_EQ(DivideRoundUp(uint8_t{255}, 2), 128);
    EXPECT_EQ(DivideRoundUp(std::numeric_limits<int>::max(), 2),
                std::numeric_limits<int>::max() / 2 + (std::numeric_limits<int>::max() % 2));
}

// DivideRoundNear unittests
TEST(DivideRoundToNearestTest, CanBeUsedAsConstexpr) {
    static constexpr int kOne = DivideRoundToNearest(5, 4);
    static constexpr int kTwo = DivideRoundToNearest(7, 4);
    static_assert(kOne == 1, "");
    static_assert(kTwo == 2, "");
}

TEST(DivideRoundToNearestTest, DivideByOddNumber) {
    EXPECT_EQ(DivideRoundToNearest(0, 3), 0);
    EXPECT_EQ(DivideRoundToNearest(1, 3), 0);
    EXPECT_EQ(DivideRoundToNearest(2, 3), 1);
    EXPECT_EQ(DivideRoundToNearest(3, 3), 1);
    EXPECT_EQ(DivideRoundToNearest(4, 3), 1);
    EXPECT_EQ(DivideRoundToNearest(5, 3), 2);
    EXPECT_EQ(DivideRoundToNearest(6, 3), 2);
}

TEST(DivideRoundToNearestTest, DivideByEvenNumberTieRoundsUp) {
    EXPECT_EQ(DivideRoundToNearest(0, 4), 0);
    EXPECT_EQ(DivideRoundToNearest(1, 4), 0);
    EXPECT_EQ(DivideRoundToNearest(2, 4), 1);
    EXPECT_EQ(DivideRoundToNearest(3, 4), 1);
    EXPECT_EQ(DivideRoundToNearest(4, 4), 1);
    EXPECT_EQ(DivideRoundToNearest(5, 4), 1);
    EXPECT_EQ(DivideRoundToNearest(6, 4), 2);
    EXPECT_EQ(DivideRoundToNearest(7, 4), 2);
}

TEST(DivideRoundToNearestTest, LargeDivisor) {
    EXPECT_EQ(DivideRoundToNearest(std::numeric_limits<int>::max() - 1,
                                 std::numeric_limits<int>::max()), 1);
}

TEST(DivideRoundToNearestTest, DivideSmallTypeByLargeType) {
    uint8_t small = 0xff;
    uint16_t large = 0xffff;
    EXPECT_EQ(DivideRoundToNearest(small, large), 0);
}
    
} // namespace test
} // namespace naivertc
