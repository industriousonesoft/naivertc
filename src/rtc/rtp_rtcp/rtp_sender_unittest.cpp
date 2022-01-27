#include "rtc/rtp_rtcp/rtp_sender.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

using namespace ::testing;

namespace naivertc {
namespace test {
namespace {

const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint32_t kRtxSsrc = 12345;
const uint32_t kFlexFecSsrc = 45678;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};
const int64_t kDefaultExpectedRetransmissionTimeMs = 125;
const size_t kMaxPaddingLength = 224;      // Value taken from rtp_sender.cc.
const uint32_t kTimestampTicksPerMs = 90;  // 90kHz clock.

class MockRtpPacketSender : public RtpPacketSender {
public:
    MOCK_METHOD(void, EnqueuePackets, (std::vector<RtpPacketToSend>), (override)); 
};
    
} // namespace

class T(RtpSenderTest) : public ::testing::Test {
public:
    T(RtpSenderTest)() 
        : clock_(123456) {}

    void SetUp() override {
        rtp_sender_ = std::make_unique<RtpSender>(GetDefaultConfig());
        rtp_sender_->SetSequenceNumberOffset(kSeqNum);
    }

    RtpConfiguration GetDefaultConfig() {
        RtpConfiguration config;
        config.local_media_ssrc = kSsrc;
        config.rtx_send_ssrc = kRtxSsrc;
        config.packet_sender = &packet_sender_;
        return config;
    }

    RtpPacketToSend BuildRtpPacket(int payload_type,
                                   bool marker_bit,
                                   uint32_t timestamp,
                                   int64_t capture_time_ms) {
        RtpPacketToSend packet = rtp_sender_->GeneratePacket();
        packet.set_packet_type(RtpPacketType::VIDEO);
        packet.set_payload_type(payload_type);
        packet.set_marker(marker_bit);
        packet.set_timestamp(timestamp);
        packet.set_capture_time_ms(capture_time_ms);
        return packet;
    }

protected:
    SimulatedClock clock_;
    NiceMock<MockRtpPacketSender> packet_sender_;
    std::unique_ptr<RtpSender> rtp_sender_;
};


MY_TEST_F(RtpSenderTest, SenderForwardsPacketsToPacer) {
    auto packet = BuildRtpPacket(kPayload, true, kTimestamp, 0);
    int64_t now_ms = clock_.now_ms();

    EXPECT_CALL(packet_sender_,
                EnqueuePackets(ElementsAre(AllOf(
                    Property(&RtpPacketToSend::ssrc, kSsrc),
                    Property(&RtpPacketToSend::sequence_number, kSeqNum),
                    Property(&RtpPacketToSend::capture_time_ms, now_ms)
                ))));
    rtp_sender_->EnqueuePacket(std::move(packet));
}
    
} // namespace test
} // namespace naivert 
