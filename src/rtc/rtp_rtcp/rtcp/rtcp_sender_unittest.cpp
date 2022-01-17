#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp"
#include "rtc/rtp_rtcp/components/rtp_receive_statistics.hpp"
#include "testing/simulated_task_queue.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

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

// MediaTransportImpl
class MediaTransportImpl : public MediaTransport {
public:
    ~MediaTransportImpl() override = default;

    int SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        return -1;
    }

    int SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        parser_.Parse(packet.data(), packet.size());
        return 0;
    }

    test::RtcpPacketParser parser_;
};

// RtpSendFeedbackProviderImpl
class RtpSendFeedbackProviderImpl : public RtpSendFeedbackProvider {
public:
    ~RtpSendFeedbackProviderImpl() override = default;

    RtpSendFeedback GetSendFeedback() override {
        return send_feedback_;
    }

    void OnRtpPacketSent(uint32_t packets_sent,
                         size_t media_bytes_sent,
                         DataRate send_bitrate = DataRate::Zero()) {
        send_feedback_.packets_sent = packets_sent;
        send_feedback_.media_bytes_sent = media_bytes_sent;
        send_feedback_.send_bitrate = send_bitrate;
    }

private:
    RtpSendFeedback send_feedback_;
};

// RtcpReceiveFeedbackProviderImpl
class RtcpReceiveFeedbackProviderImpl : public RtcpReceiveFeedbackProvider {
public:
    ~RtcpReceiveFeedbackProviderImpl() override = default;

    RtcpReceiveFeedback GetReceiveFeedback() override {
        return receive_feedback_;
    }

private:
    RtcpReceiveFeedback receive_feedback_;
    RtcpSenderReportStats last_sr_stats_;
};

// RtcpSenderTest
class T(RtcpSenderTest) : public ::testing::Test {
public:
    T(RtcpSenderTest)()
        : clock_(1'235'900'000),
          receive_statistics_(std::make_unique<RtpReceiveStatistics>(&clock_)) {}

    RtcpSender::Configuration GetDefaultConfig() {
        RtcpSender::Configuration config;
        config.audio = false;
        config.clock = &clock_;
        config.local_media_ssrc = kSenderSsrc;
        config.send_transport = &send_transport_;
        config.rtcp_report_interval_ms = 1000;
        config.report_block_provider = receive_statistics_.get();
        config.packet_type_counter_observer = nullptr;
        config.rtp_send_feedback_provider = &rtp_send_feedback_provider_;
        config.rtcp_receive_feedback_provider = &rtcp_receive_feedback_provider_;
        return config;
    }

    void InsertIncomingPacket(uint32_t remote_ssrc, uint16_t seq_num) {
        RtpPacketReceived rtp_packet;
        rtp_packet.set_ssrc(remote_ssrc);
        rtp_packet.set_sequence_number(seq_num);
        rtp_packet.set_timestamp(12345);
        rtp_packet.set_payload_type(98);
        receive_statistics_->OnRtpPacket(rtp_packet);
    }

    test::RtcpPacketParser* parser() {
        return &send_transport_.parser_;
    }

protected:
    SimulatedClock clock_;
    MediaTransportImpl send_transport_;
    std::unique_ptr<RtpReceiveStatistics> receive_statistics_;
    RtpSendFeedbackProviderImpl rtp_send_feedback_provider_;
    RtcpReceiveFeedbackProviderImpl rtcp_receive_feedback_provider_;
};

MY_TEST_F(RtcpSenderTest, SetSending) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    EXPECT_FALSE(rtcp_sender->sending());
    rtcp_sender->set_sending(true);
    EXPECT_TRUE(rtcp_sender->sending());
}

MY_TEST_F(RtcpSenderTest, SendSr) {
    const uint32_t kPacketCount = 0x12345;
    const uint32_t kOctectCount = 0x23456;

    rtp_send_feedback_provider_.OnRtpPacketSent(kPacketCount, kOctectCount);

    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_sending(true);
    NtpTime ntp = clock_.CurrentNtpTime();
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::SR));
    auto send_report = parser()->sender_report();
    EXPECT_EQ(1, send_report->num_packets());
    EXPECT_EQ(kSenderSsrc, send_report->sender_ssrc());
    EXPECT_EQ(ntp, send_report->ntp());
    EXPECT_EQ(kPacketCount, send_report->sender_packet_count());
    EXPECT_EQ(kOctectCount, send_report->sender_octet_count());
    EXPECT_EQ(kStartRtpTimestamp + kRtpTimestamp, send_report->rtp_timestamp());
    EXPECT_EQ(0u, send_report->report_blocks().size());
}
    
} // namespace test
} // namespace naivertc
