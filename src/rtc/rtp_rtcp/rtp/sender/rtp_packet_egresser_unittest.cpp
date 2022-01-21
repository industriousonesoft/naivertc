#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
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
        total_bytes_sent_ += packet.size();
        RtpPacketReceived recv_packet;
        EXPECT_TRUE(recv_packet.Parse(std::move(packet)));
        last_recv_packet_.emplace(std::move(recv_packet));
        return true;
    }

    bool SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        RTC_NOTREACHED();
    }

private:
    int64_t total_bytes_sent_;
    std::optional<RtpPacketReceived> last_recv_packet_;
};

} // namespace

class T(RtpPacketEgresserTest) : public ::testing::Test {
protected:
    T(RtpPacketEgresserTest)() 
        : time_controller_(kStartTime),
          clock_(time_controller_.Clock()),
          send_transport_(),
          packet_history_(clock_, /*enable_rtx_padding_prioritization=*/true),
          seq_num_(kStartSequenceNumber) {};

    std::unique_ptr<RtpPacketEgresser> CreateRtpSenderEgresser() {
        return std::make_unique<RtpPacketEgresser>(DefaultConfig(), &packet_history_, nullptr);
    } 

    RtpConfiguration DefaultConfig() {
        RtpConfiguration config;
        config.audio = false;
        config.clock = clock_;
        config.local_media_ssrc = kSsrc;
        config.rtx_send_ssrc = kRtxSsrc;
        config.fec_generator = nullptr;
        config.send_transport = &send_transport_;
        config.send_bitrates_observer = &send_bitrate_observer_;
        config.send_side_delay_observer = &send_delay_observer_;
        config.stream_data_counters_observer = &stream_data_counters_observer_;
        return config;
    }

    RtpPacketToSend BuildRtpPacket(bool marker_bit,
                                   int64_t capture_time_ms) {
        RtpPacketToSend packet(nullptr);
        packet.set_ssrc(kSsrc);
        packet.set_payload_type(kDefaultPayloadType);
        packet.set_packet_type(RtpPacketType::VIDEO);
        packet.set_marker(marker_bit);
        packet.set_timestamp(capture_time_ms * 90);
        packet.set_capture_time_ms(capture_time_ms);
        packet.set_sequence_number(seq_num_++);
        return packet;
    }

    RtpPacketToSend BuildRtpPacket() {
        return BuildRtpPacket(true, clock_->now_ms());
    }

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
