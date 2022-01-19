#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"
#include "testing/simulated_time_controller.hpp"

using namespace ::testing;

namespace naivertc {
namespace test {
namespace {
    
constexpr Timestamp kStartTime = Timestamp::Millis(1000'000);
constexpr int kDefaultPayloadType = 100;
constexpr int kFlexfectPayloadType = 110;
constexpr uint16_t kStartSequenceNumber = 33;
constexpr uint32_t kSsrc = 725242;
constexpr uint32_t kRtxSsrc = 12345;
constexpr uint32_t kFlexFecSsrc = 23456;

// TestConfig
struct TestConfig {
    explicit TestConfig(bool with_overhead) 
        : with_overhead(with_overhead) {}
    bool with_overhead = false;
};

// MockStreamDataCountersObserver
class MockStreamDataCountersObserver : public RtpStreamDataCountersObserver {
public:
    MOCK_METHOD(void, 
                OnStreamDataCountersUpdated, 
                (const RtpStreamDataCounters& counter, uint32_t ssrc), 
                (override));
};

// MockSendDelayObserver
class MockSendDelayObserver : public RtpSendDelayObserver {
public:
    MOCK_METHOD(void, 
                OnSendDelayUpdated, 
                (int64_t avg_delay_ms, int64_t max_delay_ms, int64_t total_delay_ms, uint32_t ssrc), 
                (override));
};

// MockSendBitratesObserver
class MockSendBitratesObserver : public RtpSendBitratesObserver {
public:
    MOCK_METHOD(void, 
                OnSendBitratesUpdated, 
                (uint32_t total_bitrate_bps, uint32_t retransmit_bitrate_bps, uint32_t ssrc), 
                (override));
};

// SendTransportImpl
class SendTransportImpl : public MediaTransport {
public:
    SendTransportImpl() 
        : total_bytes_sent_(0) {}

    bool SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        return true;
    }

    bool SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        RTC_NOTREACHED();
    }

private:
    int64_t total_bytes_sent_;
};

} // namespace

class RtpPacketEgresserTest : public ::testing::TestWithParam<TestConfig> {
protected:
    RtpPacketEgresserTest() 
        : time_controller_(kStartTime),
          clock_(time_controller_.Clock()),
          send_transport_(),
          packet_history_(clock_, /*enable_rtx_padding_prioritization=*/true),
          seq_num_(kStartSequenceNumber) {};

protected:
    SimulatedTimeController time_controller_;
    Clock* const clock_;
    NiceMock<MockStreamDataCountersObserver> stream_data_counters_observer_;
    NiceMock<MockSendBitratesObserver> send_bitrate_observer_;
    NiceMock<MockSendDelayObserver> send_delay_observer_;
    SendTransportImpl send_transport_;
    RtpPacketSentHistory packet_history_;
    uint16_t seq_num_;
};

} // namespace test
} // namespace naivertc
