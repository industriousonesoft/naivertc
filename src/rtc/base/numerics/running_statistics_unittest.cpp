#include "rtc/base/numerics/running_statistics.hpp"
#include "common/utils_random.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {
namespace {

RunningStatistics<double> CreateStatsFilledWithIntsFrom1ToN(int n) {
    std::vector<double> samples;
    for (int i = 1; i <= n; ++i) {
        samples.push_back(i);
    }
    utils::random::shuffle<double>(samples);
    RunningStatistics<double> stats;
    for (double s : samples) {
        stats.AddSample(s);
    }
    return stats;
}

RunningStatistics<double> CreateStatsFromRandom(int n, double begin, double end) {
    std::mt19937 gen{std::random_device()()};
    std::uniform_real_distribution<> dis(begin, end);
    RunningStatistics<double> stats;
    for (int i = 0; i <= n; ++i) {
        stats.AddSample(dis(gen));
    }
    return stats;
}
    
} // namespace

class T(RunningStatisticsTest) : public ::testing::TestWithParam<int> {};

constexpr int kCountForMerge = 5;

MY_TEST(RunningStatisticsTest, FullSimpleTest) {
    auto stats = CreateStatsFilledWithIntsFrom1ToN(100);

    EXPECT_DOUBLE_EQ(*stats.min(), 1.0);
    EXPECT_DOUBLE_EQ(*stats.max(), 100.0);
    ASSERT_NEAR(*stats.mean(), 50.5, 1e-10 /* abs_error */);
}

MY_TEST(RunningStatisticsTest, VarianceAndDeviation) {
    RunningStatistics<int> stats;
    stats.AddSample(2);
    stats.AddSample(2);
    stats.AddSample(-1);
    stats.AddSample(5);

    EXPECT_DOUBLE_EQ(*stats.mean(), 2.0);
    EXPECT_DOUBLE_EQ(*stats.Variance(), 4.5);
    EXPECT_DOUBLE_EQ(*stats.StandardDeviation(), sqrt(4.5));
}

MY_TEST(RunningStatisticsTest, RemoveSample) {
    RunningStatistics<int> stats;
    stats.AddSample(2);
    stats.AddSample(2);
    stats.AddSample(-1);
    stats.AddSample(5);

    const int iteration_times = 1e5;
    for (int i = 0; i < iteration_times; ++i) {
        stats.AddSample(i);
        stats.RemoveSample(i);

        EXPECT_NEAR(*stats.mean(), 2.0, 1e-8);
        EXPECT_NEAR(*stats.Variance(), 4.5, 1e-3);
        EXPECT_NEAR(*stats.StandardDeviation(), sqrt(4.5), 1e-4);
    }
}

MY_TEST(RunningStatisticsTest, RemoveSampleSequence) {
    RunningStatistics<int> stats;
    stats.AddSample(2);
    stats.AddSample(2);
    stats.AddSample(-1);
    stats.AddSample(5);

    const int iteration_times = 1e4;
    for (int i = 0; i < iteration_times; ++i) {
        stats.AddSample(i);
    }

    for (int i = 0; i < iteration_times; ++i) {
        stats.RemoveSample(i);
    }

    EXPECT_NEAR(*stats.mean(), 2.0, 1e-7);
    EXPECT_NEAR(*stats.Variance(), 4.5, 1e-3);
    EXPECT_NEAR(*stats.StandardDeviation(), sqrt(4.5), 1e-4);
}

MY_TEST(RunningStatisticsTest, VarianceFromRandom) {
    auto stats = CreateStatsFromRandom(1e6, 0, 1);
    EXPECT_NEAR(*stats.Variance(), 1.0/12, 1e-3);
}

MY_TEST(RunningStatisticsTest, NumericStabilityForVariance) {
    auto stats = CreateStatsFromRandom(1e6, 1e9, 1e9 + 1);
    EXPECT_NEAR(*stats.Variance(), 1.0/12, 1e-3);
}

MY_TEST(RunningStatisticsTest, MinRemainsUnchagedAfterRemove) {
    RunningStatistics<int> stats;
    stats.AddSample(1);
    stats.AddSample(2);
    stats.RemoveSample(1);
    EXPECT_EQ(stats.min(), 1);
}
    
MY_TEST(RunningStatisticsTest, MaxRemainsUnchagedAfterRemove) {
    RunningStatistics<int> stats;
    stats.AddSample(1);
    stats.AddSample(2);
    stats.RemoveSample(2);
    EXPECT_EQ(stats.max(), 2);
}

MY_INSTANTIATE_TEST_SUITE_P(RunningStatisticsTests,
                            RunningStatisticsTest,
                            ::testing::Range(0, kCountForMerge + 1));

MY_TEST_P(RunningStatisticsTest, MergeStatistics) {
    int samples[kCountForMerge] = {2, 2, -1, 5, 10};

    RunningStatistics<int> stats0, stats1;
    for (int i = 0; i < GetParam(); ++i) {
        stats0.AddSample(samples[i]);
    }
    for (int i = GetParam(); i < kCountForMerge; ++i) {
        stats1.AddSample(samples[i]);
    }
    stats0.Merge(stats1);

    EXPECT_EQ(stats0.sample_count(), kCountForMerge);
    EXPECT_DOUBLE_EQ(*stats0.min(), -1);
    EXPECT_DOUBLE_EQ(*stats0.max(), 10);
    EXPECT_DOUBLE_EQ(*stats0.mean(), 3.6);
    EXPECT_DOUBLE_EQ(*stats0.Variance(), 13.84);
    EXPECT_DOUBLE_EQ(*stats0.StandardDeviation(), sqrt(13.84));
}
 
} // namespace test
} // namespace naivertc
