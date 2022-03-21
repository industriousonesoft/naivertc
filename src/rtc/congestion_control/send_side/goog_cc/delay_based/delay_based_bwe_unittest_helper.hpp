#ifndef _RTC_CONGESTION_CONTROL_GOOG_CC_DELAY_BASED_BWE_UNIT_TEST_HELPER_H_
#define _RTC_CONGESTION_CONTROL_GOOG_CC_DELAY_BASED_BWE_UNIT_TEST_HELPER_H_

#include "rtc/congestion_control/base/bwe_types.hpp"
#include "rtc/congestion_control/send_side/goog_cc/probe/probe_bitrate_estimator.hpp"
#include "rtc/congestion_control/send_side/goog_cc/throughput/acknowledged_bitrate_estimator.hpp"
#include "rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe.hpp"
#include "testing/simulated_clock.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

#include <string>

namespace naivertc {
namespace test {

// TestBitrateObserver
class TestBitrateObserver {
public:
    TestBitrateObserver();
    ~TestBitrateObserver();

    bool updated() const { return updated_; }
    uint32_t latest_bitrate_bps() const { return latest_bitrate_bps_; }

    void OnReceiveBitrateChanged(uint32_t bitrate_bps);
    void Reset();

private:
    bool updated_;
    uint32_t latest_bitrate_bps_;
};

// RtpStream
class RtpStream {
public:
    enum { kSendSideOffsetUs = 1000000 /* 1s */ };

    RtpStream(int fps, int bitrate_bps);
    ~RtpStream();

    int bitrate_bps() const { return bitrate_bps_; }
    void set_bitrate_bps(int bitrate_bps) { bitrate_bps_ = bitrate_bps; }

    // The send-side time when the next frame can be generated.
    int64_t next_time_to_generate_frame_us() const { return next_time_to_generate_frame_us_; }

    std::vector<PacketResult> GenerateFrame(int64_t now_us);

    static bool Compare(const std::unique_ptr<RtpStream>& lhs,
                        const std::unique_ptr<RtpStream>& rhs);

private:
    int fps_;
    int bitrate_bps_;
    int64_t next_time_to_generate_frame_us_;
};

// RtpStreamGenerator
class RtpStreamGenerator {
public:
    RtpStreamGenerator(int link_capacity_bps, int64_t now_us);
    ~RtpStreamGenerator();

    void AddStream(std::unique_ptr<RtpStream> stream);

    void set_link_capacity_bps(int link_capacity_bps);

    // Divides |bitrate_bps| among all streams. The allocated bitrate per stream
    // is decided by the initial allocation ratios.
    void SetBitrateBps(int bitrate_bps);

    std::pair<std::vector<PacketResult>, int64_t> GenerateFrame(int64_t now_us);

private:
    // Link capacity of the simulated channel in bits per second.
    int link_capacity_bps_;
    // The time when the last packet arrived.
    int64_t pre_arrival_time_us_;
    // All streams being transmitted on this simulated channel.
    std::vector<std::unique_ptr<RtpStream>> streams_;
};

// DelayBasedBweTest
class T(DelayBasedBweTest) : public ::testing::Test {
public:
    T(DelayBasedBweTest)();
    ~T(DelayBasedBweTest)() override;

protected:
    void AddStream(int fps = 30, int bitrate_bps = 3e5 /* 300kbps */);

    void IncomingFeedback(int64_t recv_time_ms,
                          int64_t send_time_ms,
                          size_t payload_size);

    void IncomingFeedback(int64_t recv_time_ms,
                          int64_t send_time_ms,
                          size_t payload_size,
                          const PacedPacketInfo& pacing_info);

    // Generates a frame of packets belonging to a stream at a given bitrate
    // and with a given ssrc. The stream is pushed through a very simple simulated
    // network, and is then given to the receive-side bandwidth estimator.
    // Return true if an over-use was detected, false otherwise.
    bool GenerateAndProcessFrame(uint32_t ssrc, uint32_t bitrate_bps);

    // Run the bandwidth estimator with a stream of `num_of_frames` frames,
    // or until it reaches `target_bitrate`.
    // Can for instance be used to run the estimator for same time to get it
    // into a steady state.
    uint32_t SteadyStateRun(uint32_t ssrc,
                            int num_of_frames,
                            uint32_t start_bitrate,
                            uint32_t min_bitrate,
                            uint32_t max_bitrate,
                            uint32_t target_bitrate);

    void LinkCapacityDropTestHelper(int num_of_streams,
                                    uint32_t expected_bitrate_drop_delta_ms,
                                    int64_t receiver_clock_offset_change_ms);

protected:
    static const uint32_t kDefaultSsrc = 0;
    SimulatedClock clock_;
    TestBitrateObserver bitrate_observer_;
    std::unique_ptr<AcknowledgedBitrateEstimator> ack_bitrate_estimator_;
    std::unique_ptr<ProbeBitrateEstimator> probe_bitrate_estimator_;
    std::unique_ptr<DelayBasedBwe> bandwidth_estimator_;
    std::unique_ptr<RtpStreamGenerator> stream_generator_;
    int64_t recv_time_offset_ms_;
    bool first_update_;
};
    
} // namespace test
} // namespace naivertc


#endif