#include "rtc/rtp_rtcp/components/rolling_accumulator.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

#include <random>

namespace naivertc {
namespace test {
namespace {

const double kLearningRate = 0.5;

// Add `n` samples drawn from uniform distribution in [a, b].
void FillStatsFromUniformDistribution(RollingAccumulator<double>& stats, int n, double a, double b) {
    std::mt19937 gen{std::random_device()()};
    std::uniform_real_distribution<double> dist(a, b);

    for (int i = 0; i < n; ++i) {
        stats.AddSample(dist(gen));
    }
}
    
} // namespace

MY_TEST(RollingAccumulatorTest, ZeroSamples) {
    RollingAccumulator<int> accum(10);

    EXPECT_EQ(0u, accum.count());
    EXPECT_EQ(0u, accum.ComputeMean());
    EXPECT_NEAR(0u, accum.ComputeWeightedMean(kLearningRate), 0);
    EXPECT_DOUBLE_EQ(0u, accum.ComputeVariance());
    EXPECT_EQ(0u, accum.ComputeMin());
    EXPECT_EQ(0u, accum.ComputeMax());
}

MY_TEST(RollingAccumulatorTest, SomeSamples) {
    RollingAccumulator<int> accum(10);
    for (int i = 0; i < 4; ++i) {
        accum.AddSample(i);
    }

    EXPECT_EQ(4u, accum.count());
    EXPECT_EQ(1.5, accum.ComputeMean());
    EXPECT_NEAR(2.26666, accum.ComputeWeightedMean(kLearningRate), 1e-2);
    EXPECT_DOUBLE_EQ(1.25, accum.ComputeVariance());
    EXPECT_EQ(0u, accum.ComputeMin());
    EXPECT_EQ(3u, accum.ComputeMax());
}

MY_TEST(RollingAccumulatorTest, RollingSamples) {
    RollingAccumulator<int> accum(10);
    for (int i = 0; i < 12; ++i) {
        accum.AddSample(i);
    }
    EXPECT_EQ(10U, accum.count());
    EXPECT_DOUBLE_EQ(6.5, accum.ComputeMean());
    EXPECT_NEAR(10.0, accum.ComputeWeightedMean(kLearningRate), 1e-2);
    EXPECT_NEAR(9.0, accum.ComputeVariance(), 1.0);
    EXPECT_EQ(2, accum.ComputeMin());
    EXPECT_EQ(11, accum.ComputeMax());
}

MY_TEST(RollingAccumulatorTest, ResetSamples) {
    RollingAccumulator<int> accum(10);
    for (int i = 0; i < 10; ++i) {
        accum.AddSample(100);
    }
    EXPECT_EQ(10U, accum.count());
    EXPECT_DOUBLE_EQ(100.0, accum.ComputeMean());
    EXPECT_EQ(100, accum.ComputeMin());
    EXPECT_EQ(100, accum.ComputeMax());
    accum.Reset();
    EXPECT_EQ(0U, accum.count());
    for (int i = 0; i < 5; ++i) {
        accum.AddSample(i);
    }
    EXPECT_EQ(5U, accum.count());
    EXPECT_DOUBLE_EQ(2.0, accum.ComputeMean());
    EXPECT_EQ(0, accum.ComputeMin());
    EXPECT_EQ(4, accum.ComputeMax());
}

MY_TEST(RollingAccumulatorTest, RollingSamplesDouble) {
    RollingAccumulator<double> accum(10);
    for (int i = 0; i < 23; ++i) {
        accum.AddSample(5 * i);
    }
    EXPECT_EQ(10u, accum.count());
    EXPECT_DOUBLE_EQ(87.5, accum.ComputeMean());
    EXPECT_NEAR(105.049, accum.ComputeWeightedMean(kLearningRate), 1e-1);
    EXPECT_NEAR(229.166667, accum.ComputeVariance(), 25);
    EXPECT_DOUBLE_EQ(65.0, accum.ComputeMin());
    EXPECT_DOUBLE_EQ(110.0, accum.ComputeMax());
}

MY_TEST(RollingAccumulatorTest, ComputeWeightedMeanCornerCases) {
    RollingAccumulator<int> accum(10);
    EXPECT_DOUBLE_EQ(0.0, accum.ComputeWeightedMean(kLearningRate));
    EXPECT_DOUBLE_EQ(0.0, accum.ComputeWeightedMean(0.0));
    EXPECT_DOUBLE_EQ(0.0, accum.ComputeWeightedMean(1.1));
    for (int i = 0; i < 8; ++i) {
        accum.AddSample(i);
    }
    EXPECT_DOUBLE_EQ(3.5, accum.ComputeMean());
    EXPECT_DOUBLE_EQ(3.5, accum.ComputeWeightedMean(0));
    EXPECT_DOUBLE_EQ(3.5, accum.ComputeWeightedMean(1.1));
    EXPECT_NEAR(6.0, accum.ComputeWeightedMean(kLearningRate), 1e-1);
}

MY_TEST(RollingAccumulatorTest, VarianceFromUniformDistribution) {
    // Check variance converge to 1/12 for [0;1) uniform distribution.
    // Acts as a sanity check for NumericStabilityForVariance test.
    RollingAccumulator<double> stats(/*max_count=*/0.5e6);
    FillStatsFromUniformDistribution(stats, 1e6, 0, 1);
    EXPECT_NEAR(stats.ComputeVariance(), 1. / 12, 1e-3);
}

MY_TEST(RollingAccumulatorTest, NumericStabilityForVariance) {
    // Same test as VarianceFromUniformDistribution,
    // except the range is shifted to [1e9;1e9+1).
    // Variance should also converge to 1/12.
    // NB: Although we lose precision for the samples themselves, the fractional
    //     part still enjoys 22 bits of mantissa and errors should even out,
    //     so that couldn't explain a mismatch.
    RollingAccumulator<double> stats(/*max_count=*/0.5e6);
    FillStatsFromUniformDistribution(stats, 1e6, 1e9, 1e9 + 1);
    EXPECT_NEAR(stats.ComputeVariance(), 1. / 12, 1e-3);
}

} // namespace test
} // namespace naivertc
