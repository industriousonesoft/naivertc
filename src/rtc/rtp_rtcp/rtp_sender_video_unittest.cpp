#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

using namespace ::testing;

namespace naivertc {
namespace test {
namespace {
    
constexpr uint32_t kSsrc = 23456;
constexpr uint32_t kRtxSsrc = 34567;
constexpr int64_t kStartTime = 123456789;
constexpr int kPayloadType = 100;
constexpr uint32_t kTimestamp = 222;
constexpr int64_t kDefaultExpectedRetransmissionTimeMs = 123;

enum : int {
    kPlayoutDelayExtensionId = 1,
};

// RtcMediaTransportImpl
class RtcMediaTransportImpl : public RtcMediaTransport {
public:
    RtcMediaTransportImpl() {
        header_extension_map_.Register<rtp::PlayoutDelayLimits>(kPlayoutDelayExtensionId);
    }
    ~RtcMediaTransportImpl() override = default;

    bool SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        RtpPacketReceived recv_packet(&header_extension_map_);
        if (!recv_packet.Parse(packet)) {
            last_packet_.reset();
            return false;
        }
        last_packet_.emplace(std::move(recv_packet));
        return true;
    }

    bool SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        RTC_NOTREACHED();
    }

    const RtpPacketReceived* last_packet() const {
        return last_packet_ ? &last_packet_.value() : nullptr;
    }

private:
    rtp::HeaderExtensionMap header_extension_map_;
    std::optional<RtpPacketReceived> last_packet_;
};

} // namespace

// RtpSenderVideoTest
class T(RtpSenderVideoTest) : public ::testing::TestWithParam<bool> {
public:
    T(RtpSenderVideoTest)() 
        : clock_(123456) {}

    void SetUp() override {
        packet_sender_ = std::make_unique<RtpSender>(DefaultRtpConfig());
        sender_video_ = std::make_unique<RtpSenderVideo>(&clock_, packet_sender_.get());
    }

    RtpConfiguration DefaultRtpConfig() {
        RtpConfiguration config;
        config.audio = false;
        config.clock = &clock_;
        config.local_media_ssrc = kSsrc;
        config.rtx_send_ssrc = kRtxSsrc;
        config.send_transport = &send_transport_;
        return config;
    }

protected:
    SimulatedClock clock_;
    RtcMediaTransportImpl send_transport_;
    std::unique_ptr<RtpSender> packet_sender_;
    std::unique_ptr<RtpSenderVideo> sender_video_; 
};

MY_INSTANTIATE_TEST_SUITE_P(WithOrWithoutOverhead, RtpSenderVideoTest, ::testing::Bool());

MY_TEST_P(RtpSenderVideoTest, PopulatePlayoutDelay) {
    constexpr size_t kFrameSize = 123;
    uint8_t kFrame[kFrameSize];

    packet_sender_->Register(rtp::PlayoutDelayLimits::kUri, kPlayoutDelayExtensionId);
    const video::PlayoutDelay kExpectedDelay = {10, 20};

    // Send initial key-frame without playout delay.
    RtpVideoHeader video_header;
    video_header.frame_type = video::FrameType::KEY;
    video_header.codec_type = video::CodecType::H264;
    
    sender_video_->Send(kPayloadType, kTimestamp, clock_.now_ms(), video_header, kFrame, kDefaultExpectedRetransmissionTimeMs);
    clock_.AdvanceTimeMs(10);

    ASSERT_TRUE(send_transport_.last_packet() != nullptr);
    EXPECT_FALSE(send_transport_.last_packet()->HasExtension<rtp::PlayoutDelayLimits>());

    // Set playout delay on a delta frame.
    video_header.playout_delay = kExpectedDelay;
    video_header.frame_type = video::FrameType::DELTA;

    sender_video_->Send(kPayloadType, kTimestamp, clock_.now_ms(), video_header, kFrame, kDefaultExpectedRetransmissionTimeMs);
    clock_.AdvanceTimeMs(10);

    ASSERT_TRUE(send_transport_.last_packet() != nullptr);
    auto recv_playout_delay = send_transport_.last_packet()->GetExtension<rtp::PlayoutDelayLimits>();
    ASSERT_TRUE(recv_playout_delay);
    EXPECT_EQ(recv_playout_delay, kExpectedDelay);

    // Set playout delay on a delta frame, the extension should
    // still be populated since delivery wasn't guaranteed on the last frame (not a key frame).
    video_header.playout_delay = video::PlayoutDelay(); // Invalid playlout delay indicates "no change".
    sender_video_->Send(kPayloadType, kTimestamp, clock_.now_ms(), video_header, kFrame, kDefaultExpectedRetransmissionTimeMs);

    ASSERT_TRUE(send_transport_.last_packet() != nullptr);
    recv_playout_delay = send_transport_.last_packet()->GetExtension<rtp::PlayoutDelayLimits>();
    ASSERT_TRUE(recv_playout_delay);
    EXPECT_EQ(recv_playout_delay, kExpectedDelay);

    // Insert key frame, we need to refresh the state.
    video_header.frame_type = video::FrameType::KEY;
    sender_video_->Send(kPayloadType, kTimestamp, clock_.now_ms(), video_header, kFrame, kDefaultExpectedRetransmissionTimeMs);

    ASSERT_TRUE(send_transport_.last_packet() != nullptr);
    recv_playout_delay = send_transport_.last_packet()->GetExtension<rtp::PlayoutDelayLimits>();
    ASSERT_TRUE(recv_playout_delay);
    EXPECT_EQ(recv_playout_delay, kExpectedDelay);

    // The next delta frame dose not need the extension since it's delivery
    // has already been guaranteed.
    video_header.frame_type = video::FrameType::DELTA;
    sender_video_->Send(kPayloadType, kTimestamp, clock_.now_ms(), video_header, kFrame, kDefaultExpectedRetransmissionTimeMs);

    ASSERT_TRUE(send_transport_.last_packet() != nullptr);
    EXPECT_FALSE(send_transport_.last_packet()->GetExtension<rtp::PlayoutDelayLimits>());
}
    
} // namespace test
} // namespace naivertc
