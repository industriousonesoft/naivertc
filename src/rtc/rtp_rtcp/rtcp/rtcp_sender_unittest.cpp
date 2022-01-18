#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp"
#include "rtc/rtp_rtcp/components/rtp_receive_statistics.hpp"
#include "testing/simulated_task_queue.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

using namespace ::testing;

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

// MockMediaTransport
class MockMediaTransport : public MediaTransport {
public:
    MOCK_METHOD(int, SendRtpPacket, (CopyOnWriteBuffer, PacketOptions), (override));
    MOCK_METHOD(int, SendRtcpPacket, (CopyOnWriteBuffer, PacketOptions), (override));
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

    void OnReceiveTimeInfo(uint32_t ssrc, 
                           uint32_t last_rr,
                           uint32_t delay_since_last_rr) {
        rtcp::Dlrr::TimeInfo time_info;
        time_info.ssrc = ssrc;
        time_info.last_rr = last_rr;
        time_info.delay_since_last_rr = delay_since_last_rr;
        receive_feedback_.last_xr_rtis.push_back(time_info);
    }

private:
    RtcpReceiveFeedback receive_feedback_;
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
        config.rtp_send_feedback_provider = &rtp_send_feedback_provider_;
        config.rtcp_receive_feedback_provider = &rtcp_receive_feedback_provider_;
        config.packet_type_counter_observer = &packet_type_counter_observer_;
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
    RtcpPacketTypeCounterObserverImpl packet_type_counter_observer_;
};

MY_TEST_F(RtcpSenderTest, SetSending) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    EXPECT_FALSE(rtcp_sender->sending());
    rtcp_sender->set_sending(true);
    EXPECT_TRUE(rtcp_sender->sending());
}

MY_TEST_F(RtcpSenderTest, SendSr) {
    const uint32_t kPacketCount = 0x12345;
    const uint32_t kOctetCount = 0x23456;

    rtp_send_feedback_provider_.OnRtpPacketSent(kPacketCount, kOctetCount);

    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_sending(true);
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    NtpTime ntp = clock_.CurrentNtpTime();
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::SR));
    auto received_sr = parser()->sender_report();
    EXPECT_EQ(1, received_sr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_sr->sender_ssrc());
    EXPECT_EQ(ntp, received_sr->ntp());
    EXPECT_EQ(kPacketCount, received_sr->sender_packet_count());
    EXPECT_EQ(kOctetCount, received_sr->sender_octet_count());
    EXPECT_EQ(kStartRtpTimestamp + kRtpTimestamp, received_sr->rtp_timestamp());
    EXPECT_EQ(0u, received_sr->report_blocks().size());
}

MY_TEST_F(RtcpSenderTest, SendConsecutiveSrWithExactSlope) {
    const uint32_t kPacketCount = 0x12345;
    const uint32_t kOctetCount = 0x23456;
    const int kTimeBetweenSRsUs = 10043;  // Not exact value in milliseconds.
    const int kExtraPackets = 30;

    rtp_send_feedback_provider_.OnRtpPacketSent(kPacketCount, kOctetCount);

    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_sending(true);
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    clock_.AdvanceTimeUs(kTimeBetweenSRsUs);

    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::SR));
    auto send_report = parser()->sender_report();
    NtpTime ntp1 = send_report->ntp();
    uint32_t rtp1 = send_report->rtp_timestamp();

    // Send more SRs to ensure slope is always exact for different offsets.
    for (size_t packets = 1; packets <= kExtraPackets; ++packets) {
        clock_.AdvanceTimeUs(kTimeBetweenSRsUs);
        EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::SR));
        send_report = parser()->sender_report();
        EXPECT_EQ(packets + 1, send_report->num_packets());

        NtpTime ntp2 = send_report->ntp();
        uint32_t rtp2 = send_report->rtp_timestamp();

        uint32_t ntp_diff_in_rtp_uints = (ntp2.ToMs() - ntp1.ToMs()) * (kVideoPayloadTypeFrequency / 1000);
        EXPECT_EQ(rtp2 - rtp1, ntp_diff_in_rtp_uints);
    }
}

MY_TEST_F(RtcpSenderTest, DoNotSendSrBeforeRtp) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig(), /*init_timestamps=*/false);
    rtcp_sender->set_sending(true);
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    
    rtcp_sender->SendRtcp(RtcpPacketType::SR);
    EXPECT_EQ(0, parser()->sender_report()->num_packets());
    rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT);
    EXPECT_EQ(0, parser()->sender_report()->num_packets());
    // Other packets are allowed, even if useless.
    rtcp_sender->SendRtcp(RtcpPacketType::PLI);
    EXPECT_EQ(1, parser()->pli()->num_packets());
}

MY_TEST_F(RtcpSenderTest, DoNotSendCompoundBeforeRtp) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig(), /*init_timestamps=*/false);
    rtcp_sender->set_sending(true);
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);

    // In compound mode no packets are allowed when supporting sending
    // because compound mode should start with Sender Report.
    rtcp_sender->SendRtcp(RtcpPacketType::PLI);
    EXPECT_EQ(0, parser()->pli()->num_packets());
}

MY_TEST_F(RtcpSenderTest, SendRr) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig(), /*init_timestamps=*/false);
    rtcp_sender->set_sending(true);
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);

    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RR));
    auto received_rr = parser()->receiver_report();
    EXPECT_EQ(1, received_rr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_rr->sender_ssrc());
    EXPECT_EQ(0, received_rr->report_blocks().size());
}

MY_TEST_F(RtcpSenderTest, SendRrWithOneReportBlock) {
    const uint16_t kSeqNum = uint16_t(111111);
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    InsertIncomingPacket(kRemoteSsrc, kSeqNum);
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RR));
    auto received_rr = parser()->receiver_report();
    EXPECT_EQ(1, received_rr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_rr->sender_ssrc());
    EXPECT_EQ(1, received_rr->report_blocks().size());
    const auto& rb = received_rr->report_blocks()[0];
    EXPECT_EQ(kRemoteSsrc, rb.source_ssrc());
    EXPECT_EQ(0, rb.fraction_lost());
    EXPECT_EQ(0, rb.cumulative_packet_lost());
    EXPECT_EQ(kSeqNum, rb.extended_high_seq_num());
}

MY_TEST_F(RtcpSenderTest, SendRrWithTwoReportBlock) {
    const uint16_t kSeqNum = uint16_t(111111);
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    InsertIncomingPacket(kRemoteSsrc, kSeqNum);
    InsertIncomingPacket(kRemoteSsrc + 1, kSeqNum + 1);
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RR));
    auto received_rr = parser()->receiver_report();
    EXPECT_EQ(1, received_rr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_rr->sender_ssrc());
    EXPECT_EQ(2, received_rr->report_blocks().size());
    EXPECT_THAT(
      received_rr->report_blocks(),
      UnorderedElementsAre(
          Property(&rtcp::ReportBlock::source_ssrc, Eq(kRemoteSsrc)),
          Property(&rtcp::ReportBlock::source_ssrc, Eq(kRemoteSsrc + 1))));

}

MY_TEST_F(RtcpSenderTest, SendSdes) {
    const std::string kCname = "alice@host";
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->set_cname(kCname);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::SDES));
    auto received_sdes = parser()->sdes();
    EXPECT_EQ(1, received_sdes->num_packets());
    EXPECT_EQ(1, received_sdes->chunks().size());
    EXPECT_EQ(kSenderSsrc, received_sdes->chunks()[0].ssrc);
    EXPECT_EQ(kCname, received_sdes->chunks()[0].cname);
}

MY_TEST_F(RtcpSenderTest, SdesIncludedInCompoundPacket) {
    const std::string kCname = "alice@host";
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->set_cname(kCname);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT));
    auto received_sdes = parser()->sdes();
    EXPECT_EQ(1, received_sdes->num_packets());
    EXPECT_EQ(1, received_sdes->chunks().size());
    EXPECT_EQ(1, parser()->receiver_report()->num_packets());
}

MY_TEST_F(RtcpSenderTest, SendBye) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::BYE));
    EXPECT_EQ(1, parser()->bye()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->bye()->sender_ssrc());
}

MY_TEST_F(RtcpSenderTest, StopSendingTriggerBye) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->set_sending(true);
    rtcp_sender->set_sending(false);
    EXPECT_EQ(1, parser()->bye()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->bye()->sender_ssrc());
}

MY_TEST_F(RtcpSenderTest, SendFir) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::FIR));
    EXPECT_EQ(1, parser()->fir()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->fir()->sender_ssrc());
    EXPECT_EQ(1, parser()->fir()->requests().size());
    uint8_t seq = parser()->fir()->requests()[0].seq_nr;
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::FIR));
    EXPECT_EQ(2, parser()->fir()->num_packets());
    EXPECT_EQ(seq + 1, parser()->fir()->requests()[0].seq_nr);
}

MY_TEST_F(RtcpSenderTest, SendPli) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::PLI));
    EXPECT_EQ(1, parser()->pli()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->pli()->sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, parser()->pli()->media_ssrc());
}

MY_TEST_F(RtcpSenderTest, SendNack) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    std::vector<uint16_t> nack_list = {3, 12, 16};
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::NACK, nack_list.data(), nack_list.size()));
    EXPECT_EQ(1, parser()->nack()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->nack()->sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, parser()->nack()->media_ssrc());
    EXPECT_THAT(parser()->nack()->packet_ids(), ElementsAre(3, 12, 16));
}

MY_TEST_F(RtcpSenderTest, SendLossNotificationBufferingNotAllowed) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    const uint16_t kLastDecoded = 0x1234;
    const uint16_t kLastReceived = 0x4321;
    EXPECT_TRUE(rtcp_sender->SendLossNotification(kLastDecoded, kLastReceived, /*decodability_flag=*/true, /*buffering_allowed*/false));
    EXPECT_EQ(1, parser()->processed_rtcp_packets());
    EXPECT_EQ(1, parser()->loss_notification()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->loss_notification()->sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, parser()->loss_notification()->media_ssrc());
}

MY_TEST_F(RtcpSenderTest, SendLossNotificationBufferingAllowed) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    const uint16_t kLastDecoded = 0x1234;
    const uint16_t kLastReceived = 0x4321;
    EXPECT_TRUE(rtcp_sender->SendLossNotification(kLastDecoded, kLastReceived, /*decodability_flag=*/true, /*buffering_allowed*/true));
    EXPECT_EQ(0, parser()->processed_rtcp_packets());

    // Sending another messages triggers sending the LNTF messages as well.
    std::vector<uint16_t> nack_list = {3, 12, 16};
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::NACK, nack_list.data(), nack_list.size()));
    EXPECT_EQ(1, parser()->processed_rtcp_packets());
    EXPECT_EQ(kSenderSsrc, parser()->loss_notification()->sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, parser()->loss_notification()->media_ssrc());
    EXPECT_EQ(1, parser()->nack()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser()->nack()->sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, parser()->nack()->media_ssrc());
}

MY_TEST_F(RtcpSenderTest, RembNotIncludedBeforeSet) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);

    rtcp_sender->SendRtcp(RtcpPacketType::RR);

    ASSERT_EQ(1, parser()->receiver_report()->num_packets());
    EXPECT_EQ(0, parser()->remb()->num_packets());
}

MY_TEST_F(RtcpSenderTest, RembNotIncludedAfterUnset) {
    const int64_t kBitrateBps = 202201;
    const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->SetRemb(kBitrateBps, kSsrcs);
    rtcp_sender->SendRtcp(RtcpPacketType::RR);
    ASSERT_EQ(1, parser()->receiver_report()->num_packets());
    EXPECT_EQ(1, parser()->remb()->num_packets());

    // Turn off REMB, rtcp_sender no longer should send it.
    rtcp_sender->UnsetRemb();
    rtcp_sender->SendRtcp(RtcpPacketType::RR);
    ASSERT_EQ(2, parser()->receiver_report()->num_packets());
    EXPECT_EQ(1, parser()->remb()->num_packets());
}

MY_TEST_F(RtcpSenderTest, SendRemb) {
    const int64_t kBitrateBps = 202201;
    const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->SetRemb(kBitrateBps, kSsrcs);

    rtcp_sender->SendRtcp(RtcpPacketType::REMB);

    auto received_remb = parser()->remb();
    EXPECT_EQ(1, received_remb->num_packets());
    EXPECT_EQ(kSenderSsrc, received_remb->sender_ssrc());
    EXPECT_EQ(kBitrateBps, received_remb->bitrate_bps());
    EXPECT_THAT(received_remb->ssrcs(), ElementsAre(kRemoteSsrc, kRemoteSsrc + 1));
}

MY_TEST_F(RtcpSenderTest, RembIncludedInEachCompoundPacketAfterSet) {
    const int64_t kBitrateBps = 202201;
    const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->SetRemb(kBitrateBps, kSsrcs);

    rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT);
    EXPECT_EQ(1, parser()->remb()->num_packets());
    // REMB should be included in each compound packet.
    rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT);
    EXPECT_EQ(2, parser()->remb()->num_packets());
}

MY_TEST_F(RtcpSenderTest, SendXrWithDlrr) {
    const uint32_t kSsrc = 0x111111;
    const uint32_t kLastRr = 0x222222;
    const uint32_t kDelaySinceLastRr = 0x333333;

    rtcp_receive_feedback_provider_.OnReceiveTimeInfo(kSsrc, kLastRr, kDelaySinceLastRr);

    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);

    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT));

    auto received_xr = parser()->xr();
    EXPECT_EQ(1, received_xr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_xr->sender_ssrc());
    auto& time_infos = received_xr->dlrr().time_infos();
    ASSERT_EQ(1, time_infos.size());
    EXPECT_EQ(kSsrc, time_infos[0].ssrc);
    EXPECT_EQ(kLastRr, time_infos[0].last_rr);
    EXPECT_EQ(kDelaySinceLastRr, time_infos[0].delay_since_last_rr);
}

MY_TEST_F(RtcpSenderTest, SendXrWithMultipleDlrrTimeInfos) {
    const size_t kNumReceivers = 2;
    for (size_t i = 0; i < kNumReceivers; ++i) {
        rtcp_receive_feedback_provider_.OnReceiveTimeInfo(i, (i + 1) * 100, (i + 2) * 200);
    }

    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);

    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT));

    auto received_xr = parser()->xr();
    EXPECT_EQ(1, received_xr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_xr->sender_ssrc());
    auto& time_infos = received_xr->dlrr().time_infos();
    ASSERT_EQ(kNumReceivers, time_infos.size());

    for (size_t i = 0; i < kNumReceivers; ++i) {
        EXPECT_EQ(i, time_infos[i].ssrc);
        EXPECT_EQ((i + 1) * 100, time_infos[i].last_rr);
        EXPECT_EQ((i + 2) * 200, time_infos[i].delay_since_last_rr);
    }
}

MY_TEST_F(RtcpSenderTest, SendXrWithRrtr) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);
    rtcp_sender->set_sending(false);
    auto ntp = clock_.CurrentNtpTime();
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT));
    auto received_xr = parser()->xr();
    EXPECT_EQ(1, received_xr->num_packets());
    EXPECT_EQ(kSenderSsrc, received_xr->sender_ssrc());
    EXPECT_FALSE(received_xr->dlrr());
    ASSERT_TRUE(received_xr->rrtr());
    EXPECT_EQ(ntp, received_xr->rrtr()->ntp());
}

MY_TEST_F(RtcpSenderTest, DoNotSendXrWithRrtrIfSending) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);
    rtcp_sender->set_sending(true);
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::RTCP_REPORT));
    EXPECT_EQ(0, parser()->xr()->num_packets());
}

MY_TEST_F(RtcpSenderTest, ByeMustBeTheLastToSend) {
    MockMediaTransport mock_transport;
    EXPECT_CALL(mock_transport, SendRtcpPacket(_, _))
        .WillOnce(Invoke([](CopyOnWriteBuffer packet, PacketOptions options) {
            const uint8_t* next_packet = packet.data();
            const uint8_t* packet_end = packet.data() + packet.size();
            rtcp::CommonHeader rtcp_block;
            while (next_packet < packet_end) {
                EXPECT_TRUE(rtcp_block.Parse(next_packet, packet_end - next_packet));
                next_packet = rtcp_block.NextPacket();
                if (rtcp_block.type() == rtcp::Bye::kPacketType) {
                    EXPECT_EQ(0, packet_end - next_packet) << "Bye packet should be last in a compound RTCP packet.";
                }
                // Validate test was set correctly.
                if (next_packet == packet_end) {
                    EXPECT_EQ(rtcp_block.type(), rtcp::Bye::kPacketType) << "Last packet in this test expected to be Bye.";
                }
            }
            return true;
        }));

    RtcpSender::Configuration config = GetDefaultConfig();
    config.send_transport = &mock_transport;

    auto rtcp_sender = CreateRtcpSender(config);
    rtcp_sender->SetTimestampOffset(kStartRtpTimestamp);
    rtcp_sender->SetLastRtpTime(kRtpTimestamp, clock_.CurrentTime(), /*paylaod_type=*/98);

    rtcp_sender->set_rtcp_mode(RtcpMode::COMPOUND);
    rtcp_sender->SetRemb(1234, {});
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::BYE));
}

MY_TEST_F(RtcpSenderTest, PacketTypeObserver) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);

    int64_t now_ms = clock_.now_ms();
    EXPECT_TRUE(rtcp_sender->SendRtcp(RtcpPacketType::PLI));
    EXPECT_EQ(1, parser()->pli()->num_packets());
    EXPECT_EQ(kRemoteSsrc, packet_type_counter_observer_.ssrc_);
    EXPECT_EQ(1, packet_type_counter_observer_.packet_counter_.pli_packets);
    EXPECT_EQ(now_ms, packet_type_counter_observer_.packet_counter_.first_packet_time_ms);
}

MY_TEST_F(RtcpSenderTest, DoesntSchedulesInitialReportWhenSsrcSetOnConstruction) {
    auto rtcp_sender = CreateRtcpSender(GetDefaultConfig());
    rtcp_sender->set_rtcp_mode(RtcpMode::REDUCED_SIZE);
    rtcp_sender->set_remote_ssrc(kRemoteSsrc);

    // New report should not have been scheduled yet.
    clock_.AdvanceTimeMs(100);
    EXPECT_FALSE(rtcp_sender->TimeToSendRtcpReport(/*send_rtcp_before_key_frame=*/false));
}
    
} // namespace test
} // namespace naivertc
