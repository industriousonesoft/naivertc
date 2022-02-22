#include "rtc/congestion_controller/components/interval_budget.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {
    
constexpr int64_t kWindowMs = 500;
constexpr DataRate kTargetBitrate = DataRate::KilobitsPerSec(100);

size_t IntervalBytes(DataRate bitrate, int64_t interval_ms) {
    return static_cast<size_t>((bitrate.kbps() * interval_ms) / 8);
}

int64_t IntervalTimeMs(DataRate bitrate, size_t bytes) {
    return static_cast<int64_t>((bytes * 8) / bitrate.kbps<double>());
}

} // namespace

class T(IntervalBudgetTest) : public ::testing::TestWithParam<bool> {
public:
    T(IntervalBudgetTest)() 
        : interval_budget_(kTargetBitrate, /*can_build_up_from_underuse=*/GetParam()) {}

protected:
    IntervalBudget interval_budget_;
};

MY_INSTANTIATE_TEST_SUITE_P(WithAndWithoutCanBuildUpFromUnderuse, IntervalBudgetTest, ::testing::Bool());

MY_TEST_P(IntervalBudgetTest, InitialConfiguration) {
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 0.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, Underuse) {
    int64_t interval_time_ms = 50;
    // Increase budget
    interval_budget_.IncreaseBudget(interval_time_ms);

    // Consume budget but still in underuse
    int consumed_bytes = 600; // 48ms
    int consumed_time_ms = IntervalTimeMs(kTargetBitrate, consumed_bytes);
    interval_budget_.ConsumeBudget(consumed_bytes);

    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), static_cast<double>(interval_time_ms - consumed_time_ms) / kWindowMs) 
                    << consumed_time_ms;
    EXPECT_GT(interval_budget_.bytes_remaining(), 0u);
    EXPECT_EQ(interval_budget_.bytes_remaining(), IntervalBytes(kTargetBitrate, interval_time_ms - consumed_time_ms));

    // Continue consuming 2ms
    interval_budget_.ConsumeBudget(25);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 0);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, DontUnderuseMoreThanMaxWindow) {
    int interval_time_ms = 1000;
    interval_budget_.IncreaseBudget(interval_time_ms);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 1.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, kWindowMs));
}

MY_TEST_P(IntervalBudgetTest, DontUnderuseMoreThanMaxWindowWhenChangeBitrate) {
    int interval_time_ms = kWindowMs / 2;
    interval_budget_.IncreaseBudget(interval_time_ms);
    interval_budget_.set_target_bitrate(kTargetBitrate / 10);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 1.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate / 10, kWindowMs));
}

MY_TEST_P(IntervalBudgetTest, BalanceChangeOnBitrateChange) {
    int interval_time_ms = kWindowMs;
    interval_budget_.IncreaseBudget(interval_time_ms);
    interval_budget_.set_target_bitrate(kTargetBitrate * 2);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 0.5);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, kWindowMs));
}

MY_TEST_P(IntervalBudgetTest, Overuse) {
    int overuse_time_ms = 50;
    int consumed_bytes = IntervalBytes(kTargetBitrate, overuse_time_ms);
    interval_budget_.ConsumeBudget(consumed_bytes);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                     static_cast<double>(overuse_time_ms) / -kWindowMs);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, DontOveruseMoreThanMaxWindow) {
    int overuse_time_ms = 1000;
    int consumed_bytes = IntervalBytes(kTargetBitrate, overuse_time_ms);
    interval_budget_.ConsumeBudget(consumed_bytes);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), -1.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, CanBuildUpFromUnderuseWhenConfigured) {
    int interval_time_ms = 50;
    // Increase budget
    interval_budget_.IncreaseBudget(interval_time_ms);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                    static_cast<double>(interval_time_ms) /kWindowMs);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, interval_time_ms));

    // Consume budget but still in underuse
    int consumed_bytes = 100;
    int consumed_time_ms = IntervalTimeMs(kTargetBitrate, consumed_bytes);
    interval_budget_.ConsumeBudget(consumed_bytes);

    // Increase budget
    interval_budget_.IncreaseBudget(interval_time_ms);
    
    if (GetParam()) {
        // Build up budget from underuse.
        EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                    static_cast<double>(2 * interval_time_ms - consumed_time_ms) / kWindowMs );
        EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, 2 * interval_time_ms) - consumed_bytes);
    } else {
        EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                    static_cast<double>(interval_time_ms) / kWindowMs );
        EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, interval_time_ms));
    }
    
}
    
} // namespace test
} // namespace naivertc
