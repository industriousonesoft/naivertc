#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_UNIT_TEST_HELPER_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_UNIT_TEST_HELPER_H_

#include "rtc/congestion_controller/network_types.hpp"
#include "rtc/congestion_controller/goog_cc/acknowledged_bitrate_estimator.hpp"
#include "rtc/congestion_controller/goog_cc/delay_based_bwe.hpp"
#include "testing/simulated_clock.hpp"

#include <gtest/gtest.h>

#include <string>

namespace naivertc {
namespace test {

// TestBitrateObserver
class TestBitrateObserver {
public:
    TestBitrateObserver();
    ~TestBitrateObserver();

    bool updated() const { return updated_; }
    bool latest_bitrate() const { return latest_bitrate_; }

    void OnReceiveBitrateChanged(uint32_t bitrate);
    void Reset();

private:
    bool updated_;
    uint32_t latest_bitrate_;
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
    int64_t next_rtp_time_us() const { return next_rtp_time_us_; }

    std::vector<PacketResult> GenerateFrame(int64_t now_us);

    static bool Compare(const std::unique_ptr<RtpStream>& lhs,
                        const std::unique_ptr<RtpStream>& rhs);

private:
    int fps_;
    int bitrate_bps_;
    int64_t next_rtp_time_us_;
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
class DelayBasedBweTest : public ::testing::TestWithParam<std::string> {
public:

protected:
    SimulatedClock clock_;
    TestBitrateObserver bitrate_observer_;
    std::unique_ptr<AcknowledgedBitrateEstimator> ack_bitrate_estimator_;
    
};
    
} // namespace test
} // namespace naivertc


#endif