#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "testing/simulated_clock.hpp"
#include "rtc/base/time/ntp_time_util.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

// SSRC of remote peer, that sends rtcp packet to the rtcp receiver under test.
constexpr uint32_t kSenderSsrc = 0x10203;
// SSRCs of local peer, that rtcp packet addressed to.
constexpr uint32_t kReceiverMainSsrc = 0x123456;
// RtcpReceiver can accept several ssrc, e.g. regular and rtx streams.
constexpr uint32_t kReceiverExtraSsrc = 0x1234567;
// SSRCs to ignore (i.e. not configured in RtcpReceiver).
constexpr uint32_t kNotToUsSsrc = 0x654321;
constexpr uint32_t kUnknownSenderSsrc = 0x54321;
constexpr int64_t kRtcpIntervalMs = 1000;

class MockRtcpPacketTypeCounterObserver : public RtcpPacketTypeCounterObserver {
public:
    MOCK_METHOD(void, 
                RtcpPacketTypesCounterUpdated, 
                (uint32_t, const RtcpPacketTypeCounter&), 
                (override));
};

class MockRtcpIntraFrameObserver : public RtcpIntraFrameObserver {
public:
    MOCK_METHOD(void, OnReceivedIntraFrameRequest, (uint32_t), (override));
};

class MockRtcpLossNotificationObserver : public RtcpLossNotificationObserver {
public:
    MOCK_METHOD(void,
                OnReceivedLossNotification,
                (uint32_t ssrc,
                uint16_t seq_num_of_last_decodable,
                uint16_t seq_num_of_last_received,
                bool decodability_flag),
                (override));
};

class MockCnameObserver : public RtcpCnameObserver {
public:
    MOCK_METHOD(void, OnCname, (uint32_t, std::string_view), (override));
};

class MockTransportFeedbackObserver : public RtcpTransportFeedbackObserver {
public:
    MOCK_METHOD(void,
                OnTransportFeedback,
                (const rtcp::TransportFeedback&),
                (override));
};

class MockRtcpReceiverObserver : public RtcpReceiver::Observer {
public:
    MOCK_METHOD(void, OnRequestSendReport, (), (override));
    MOCK_METHOD(void, OnReceivedNack, (const std::vector<uint16_t>&), (override));
    MOCK_METHOD(void,
                OnReceivedRtcpReportBlocks,
                (const std::vector<RtcpReportBlock>&),
                (override));
};
    
} // namespace


struct ReceiverMocks {
    ReceiverMocks() : clock(1'335'900'000) {}

    SimulatedClock clock;

    NiceMock<MockRtcpPacketTypeCounterObserver> packet_type_counter_observer;
    StrictMock<MockRtcpIntraFrameObserver> intra_frame_observer;
    StrictMock<MockRtcpLossNotificationObserver> loss_notification_observer;
    StrictMock<MockTransportFeedbackObserver> transport_feedback_observer;
    StrictMock<MockRtcpReceiverObserver> rtcp_receiver_observer;
};

RtcpConfiguration DefaultConfiguration(ReceiverMocks* mocks) {
    RtcpConfiguration config;

    config.rtcp_report_interval_ms = kRtcpIntervalMs;
    config.local_media_ssrc = kReceiverMainSsrc;
    config.rtx_send_ssrc = kReceiverExtraSsrc;

    config.clock = &mocks->clock;
    config.receiver_only = false;
    config.packet_type_counter_observer = &mocks->packet_type_counter_observer;
    config.intra_frame_observer = &mocks->intra_frame_observer;
    config.loss_notification_observer = &mocks->loss_notification_observer;
    config.transport_feedback_observer = &mocks->transport_feedback_observer;

    return config;
}

MY_TEST(RtcpReceiverTest, BrokenPacketIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);

    const uint8_t bad_packet[] = {0, 0, 0, 0};
    EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated).Times(0);
    receiver.IncomingPacket(bad_packet);
}

MY_TEST(RtcpReceiverTest, InvalidFeedbackIsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);

    const uint8_t bad_packet[] = {0x81, rtcp::Rtpfb::kPacketType, 0, 0};
    EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated).Times(0);
    EXPECT_CALL(mocks.transport_feedback_observer, OnTransportFeedback).Times(0);
    receiver.IncomingPacket(bad_packet);
}

MY_TEST(RtcpReceiverTest, InjectSrPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    EXPECT_FALSE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr));

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(IsEmpty()));
    receiver.IncomingPacket(sr.Build());

    EXPECT_TRUE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr, 
                             nullptr, nullptr, nullptr));
}

MY_TEST(RtcpReceiverTest, InjectSrPacketFromUnknownSender) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kUnknownSenderSsrc);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingPacket(sr.Build());

    EXPECT_FALSE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr, 
                              nullptr, nullptr, nullptr));
}

MY_TEST(RtcpReceiverTest, InjectSrPacketCalculatesRTT) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    const int64_t kRttMs = 123;
    const uint32_t kDelayNtp = 0x4321;
    const int64_t kDelayMs = CompactNtpRttToMs(kDelayNtp);

    int64_t rtt_ms = 123;
    EXPECT_EQ(-1, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));

    uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
    mocks.clock.AdvanceTimeMs(kRttMs + kDelayMs);

    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock block;
    block.set_media_ssrc(kReceiverMainSsrc);
    block.set_last_sr_ntp_timestamp(sent_ntp);
    block.set_delay_sr_since_last_sr(kDelayNtp);
    sr.AddReportBlock(block);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingPacket(sr.Build());

    EXPECT_EQ(0, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));
    EXPECT_NEAR(kRttMs, rtt_ms, 1);

}

MY_TEST(RtcpReceiverTest, InjectSrPacketCalculatesNegativeRTTAsOne) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    const int64_t kRttMs = -13;
    const uint32_t kDelayNtp = 0x4321;
    const int64_t kDelayMs = CompactNtpRttToMs(kDelayNtp);

    int64_t rtt_ms = 0;
    EXPECT_EQ(-1, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));

    uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
    mocks.clock.AdvanceTimeMs(kRttMs + kDelayMs);

    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock block;
    block.set_media_ssrc(kReceiverMainSsrc);
    block.set_last_sr_ntp_timestamp(sent_ntp);
    block.set_delay_sr_since_last_sr(kDelayNtp);
    sr.AddReportBlock(block);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingPacket(sr.Build());

    EXPECT_EQ(0, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));
    EXPECT_EQ(1, rtt_ms);
}

MY_TEST(RtcpReceiverTest, TwoReportBlocksWithLastOneWithoutLastSrCalculatesRttForBandwidthObserver) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    const int64_t kRttMs = 120;
    const uint32_t kDelayNtp = 123000;
    const int64_t kDelayMs = CompactNtpRttToMs(kDelayNtp);

    uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
    mocks.clock.AdvanceTimeMs(kRttMs + kDelayMs);

    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock block;
    block.set_media_ssrc(kReceiverMainSsrc);
    block.set_last_sr_ntp_timestamp(sent_ntp);
    block.set_delay_sr_since_last_sr(kDelayNtp);
    sr.AddReportBlock(block);
    block.set_media_ssrc(kReceiverExtraSsrc);
    block.set_last_sr_ntp_timestamp(0);
    sr.AddReportBlock(block);
    
    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(2)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(2), kRttMs, _));
    receiver.IncomingPacket(sr.Build());
}

MY_TEST(RtcpReceiverTest, InjectRrPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(IsEmpty()));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(IsEmpty(), _, now_ms));
    receiver.IncomingPacket(rr.Build());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), IsEmpty());
}

MY_TEST(RtcpReceiverTest, InjectRrPacketWithReportBlockNotToUsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kNotToUsSsrc);
    rr.AddReportBlock(rb);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(IsEmpty()));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(IsEmpty(), _, now_ms));
    receiver.IncomingPacket(rr.Build());

    EXPECT_EQ(0, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), IsEmpty());
}

MY_TEST(RtcpReceiverTest, InjectRrPacketWithOneReportBlock) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kReceiverMainSsrc);
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rr.AddReportBlock(rb);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(1)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(1), _, now_ms));

    receiver.IncomingPacket(rr.Build());
    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(1));
}

MY_TEST(RtcpReceiverTest, InjectSrPacketWithOneReportBlock) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kReceiverMainSsrc);
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    sr.AddReportBlock(rb);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(1)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(1), _, now_ms));

    receiver.IncomingPacket(sr.Build());
    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(1));
}

MY_TEST(RtcpReceiverTest, InjectRrPacketWithTwoReportBlocks) {
    const uint16_t kSequenceNumbers[] = {10, 12423};
    const uint32_t kCumLost[] = {13, 555};
    const uint8_t kFracLost[] = {20, 11};
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReportBlock rb1;
    rb1.set_media_ssrc(kReceiverMainSsrc);
    rb1.set_extended_highest_sequence_num(kSequenceNumbers[0]);
    rb1.set_fraction_lost(10);

    rtcp::ReportBlock rb2;
    rb2.set_media_ssrc(kReceiverExtraSsrc);
    rb2.set_extended_highest_sequence_num(kSequenceNumbers[1]);
    rb2.set_fraction_lost(0);

    rtcp::ReceiverReport rr1;
    rr1.set_sender_ssrc(kSenderSsrc);
    rr1.AddReportBlock(rb1);
    rr1.AddReportBlock(rb2);
    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(2)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(2), _, now_ms));
    receiver.IncomingPacket(rr1.Build());
    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(),
                UnorderedElementsAre(
                    Field(&RtcpReportBlock::fraction_lost, 0),
                    Field(&RtcpReportBlock::fraction_lost, 10)));
    // Insert next receiver report with same ssrc but new values.
    rtcp::ReportBlock rb3;
    rb3.set_media_ssrc(kReceiverMainSsrc);
    rb3.set_extended_highest_sequence_num(kSequenceNumbers[0]);
    rb3.set_fraction_lost(kFracLost[0]);
    rb3.set_cumulative_packet_lost(kCumLost[0]);

    rtcp::ReportBlock rb4;
    rb4.set_media_ssrc(kReceiverExtraSsrc);
    rb4.set_extended_highest_sequence_num(kSequenceNumbers[1]);
    rb4.set_fraction_lost(kFracLost[1]);
    rb4.set_cumulative_packet_lost(kCumLost[1]);

    rtcp::ReceiverReport rr2;
    rr2.set_sender_ssrc(kSenderSsrc);
    rr2.AddReportBlock(rb3);
    rr2.AddReportBlock(rb4);
    // Advance time to make 1st sent time and 2nd sent time different.
    mocks.clock.AdvanceTimeMs(500);
    now_ms = mocks.clock.now_ms();
    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(2)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(2), _, now_ms));
    receiver.IncomingPacket(rr2.Build());
    EXPECT_THAT(
        receiver.GetLatestReportBlocks(),
        UnorderedElementsAre(
            AllOf(Field(&RtcpReportBlock::source_ssrc, kReceiverMainSsrc),
                        Field(&RtcpReportBlock::fraction_lost, kFracLost[0]),
                        Field(&RtcpReportBlock::packets_lost, kCumLost[0]),
                        Field(&RtcpReportBlock::extended_highest_sequence_number,
                            kSequenceNumbers[0])),
            AllOf(Field(&RtcpReportBlock::source_ssrc, kReceiverExtraSsrc),
                        Field(&RtcpReportBlock::fraction_lost, kFracLost[1]),
                        Field(&RtcpReportBlock::packets_lost, kCumLost[1]),
                        Field(&RtcpReportBlock::extended_highest_sequence_number,
                            kSequenceNumbers[1]))));
}

MY_TEST(RtcpReceiverTest, InjectRrPacketsFromTwoRemoteSsrcsReturnsLatestReportBlock) {
    const uint32_t kSenderSsrc2 = 0x20304;
    const uint16_t kSequenceNumbers[] = {10, 12423};
    const int32_t kCumLost[] = {13, 555};
    const uint8_t kFracLost[] = {20, 11};

    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::ReportBlock rb1;
    rb1.set_media_ssrc(kReceiverMainSsrc);
    rb1.set_extended_highest_sequence_num(kSequenceNumbers[0]);
    rb1.set_fraction_lost(kFracLost[0]);
    rb1.set_cumulative_packet_lost(kCumLost[0]);

    rtcp::ReceiverReport rr1;
    rr1.set_sender_ssrc(kSenderSsrc);
    rr1.AddReportBlock(rb1);

    int64_t now_ms = mocks.clock.now_ms();
    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(1)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(1), _, now_ms));
    receiver.IncomingPacket(rr1.Build());

    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(
        receiver.GetLatestReportBlocks(),
        UnorderedElementsAre(
            AllOf(Field(&RtcpReportBlock::source_ssrc, kReceiverMainSsrc),
                  Field(&RtcpReportBlock::sender_ssrc, kSenderSsrc),
                  Field(&RtcpReportBlock::fraction_lost, kFracLost[0]),
                  Field(&RtcpReportBlock::packets_lost, kCumLost[0]),
                  Field(&RtcpReportBlock::extended_highest_sequence_number, kSequenceNumbers[0]))));

    rtcp::ReportBlock rb2;
    rb2.set_media_ssrc(kReceiverMainSsrc);
    rb2.set_extended_highest_sequence_num(kSequenceNumbers[1]);
    rb2.set_fraction_lost(kFracLost[1]);
    rb2.set_cumulative_packet_lost(kCumLost[1]);

    rtcp::ReceiverReport rr2;
    rr2.set_sender_ssrc(kSenderSsrc2);
    rr2.AddReportBlock(rb2);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(SizeIs(1)));
    // EXPECT_CALL(mocks.bandwidth_observer,
    //             OnReceivedRtcpReceiverReport(SizeIs(1), _, now));
    receiver.IncomingPacket(rr2.Build());

    EXPECT_THAT(
        receiver.GetLatestReportBlocks(),
        UnorderedElementsAre(
            AllOf(Field(&RtcpReportBlock::source_ssrc, kReceiverMainSsrc),
                  Field(&RtcpReportBlock::sender_ssrc, kSenderSsrc2),
                  Field(&RtcpReportBlock::fraction_lost, kFracLost[1]),
                  Field(&RtcpReportBlock::packets_lost, kCumLost[1]),
                  Field(&RtcpReportBlock::extended_highest_sequence_number, kSequenceNumbers[1]))));
}

MY_TEST(RtcpReceiverTest, GetRtt) {
    const uint32_t kSentCompactNtp = 0x1234;
    const uint32_t kDelayCompactNtp = 0x222;

    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    // No report block received.
    EXPECT_EQ(-1, receiver.RTT(kSenderSsrc, nullptr, nullptr, nullptr, nullptr));

    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kReceiverMainSsrc);
    rb.set_last_sr_ntp_timestamp(kSentCompactNtp);
    rb.set_delay_sr_since_last_sr(kDelayCompactNtp);
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rr.AddReportBlock(rb);
    int64_t now_ms = mocks.clock.now_ms();

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    // EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
    receiver.IncomingPacket(rr.Build());

    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_EQ(0, receiver.RTT(kSenderSsrc, nullptr, nullptr, nullptr, nullptr));
}

MY_TEST(RtcpReceiverTest, InjectSdesWithOneChunk) {
    ReceiverMocks mocks;
    MockCnameObserver cname_observer;
    RtcpConfiguration config = DefaultConfiguration(&mocks);
    config.cname_observer = &cname_observer;

    RtcpReceiver receiver(config, &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    const char kCname[] = "alice@host";
    rtcp::Sdes sdes;
    sdes.AddCName(kSenderSsrc, kCname);

    EXPECT_CALL(cname_observer, OnCname(kSenderSsrc, StrEq(kCname)));
    receiver.IncomingPacket(sdes.Build());
}

MY_TEST(RtcpReceiverTest, InjectByePacket_RemovesReportBlocks) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::ReportBlock rb1;
    rb1.set_media_ssrc(kReceiverMainSsrc);
    rtcp::ReportBlock rb2;
    rb2.set_media_ssrc(kReceiverExtraSsrc);
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rr.AddReportBlock(rb1);
    rr.AddReportBlock(rb2);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    // EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
    receiver.IncomingPacket(rr.Build());

    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(2));
    // Verify that BYE removes the report blocks.
    rtcp::Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);
    receiver.IncomingPacket(bye.Build());

    EXPECT_THAT(receiver.GetLatestReportBlocks(), IsEmpty());
    // Inject packet again.
    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    // EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
    receiver.IncomingPacket(rr.Build());

    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(2));
}
    
} // namespace test
} // namespace naivertc

