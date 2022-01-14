#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr uint32_t kSenderSsrc = 0x11111111;
constexpr uint32_t kRemoteSsrc = 0x22222222;
constexpr uint32_t kStartRtpTimestamp = 0x34567;
constexpr uint32_t kRtpTimestamp = 0x45678;

std::unique_ptr<RtcpSender> CreateRtcpSender(const RtcpSender::Configuration& config, 
                                             bool init_timestamps = true) {
    auto rtcp_sender = std::make_unique<RtcpSender>(config);
    rtcp_sender->set_remote_ssrc(kRemoteSsrc);
    if (init_timestamps) {
        rtcp_sender->SetTimestampOffset(kStartRtpTimestamp);
        rtcp_sender->SetLastRtpTime(kRtpTimestamp, config.clock->CurrentTime(), 0);
    }
    return rtcp_sender;
}
    
} // namespace

// RtcpPacketTypeCounterObserverImpl
class RtcpPacketTypeCounterObserverImpl : public RtcpPacketTypeCounterObserver {
public:
    RtcpPacketTypeCounterObserverImpl() : ssrc_(0) {};
    ~RtcpPacketTypeCounterObserverImpl() override = default;

    void RtcpPacketTypesCounterUpdated(uint32_t ssrc,
                                       const RtcpPacketTypeCounter& packet_counter) override {
        ssrc_ = ssrc;
        packet_counter_ = packet_counter;
    }
    uint32_t ssrc_;
    RtcpPacketTypeCounter packet_counter_;
};

// TesetTransport
class TesetTransport : public MediaTransport {
public:
    TesetTransport() = default;
    ~TesetTransport() override = default;

    int SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        return -1;
    }

    int SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        parser_.Parse(packet.data(), packet.size());
        return 0;
    }

    test::RtcpPacketParser parser_;
};

// RtcpSenderTest
class RtcpSenderTest : public ::testing::Test {

};
    
} // namespace test
} // namespace naivertc
