#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "testing/simulated_clock.hpp"
#include "rtc/base/time/ntp_time_util.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/compound_packet.hpp"
#include "rtc/base/arraysize.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
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

class MockRtcpBandwidthObserver : public RtcpBandwidthObserver {
public:
    MOCK_METHOD(void, OnReceivedEstimatedBitrateBps, (uint32_t), (override));
};

class MockRtcpNackListObserver : public RtcpNackListObserver {
public:
    MOCK_METHOD(void, OnReceivedNack, (const std::vector<uint16_t>&, int64_t), (override));
};

class MockRtcpReportBlocksObserver : public RtcpReportBlocksObserver {
public:
    MOCK_METHOD(void,
                OnReceivedRtcpReportBlocks,
                (const std::vector<RtcpReportBlock>&, int64_t),
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
    StrictMock<MockRtcpBandwidthObserver> bandwidth_observer;
    StrictMock<MockRtcpNackListObserver> nack_list_observer;
    StrictMock<MockRtcpReportBlocksObserver> report_blocks_observer;
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
    config.bandwidth_observer = &mocks->bandwidth_observer;
    config.nack_list_observer = &mocks->nack_list_observer;
    config.report_blocks_observer = &mocks->report_blocks_observer;
    return config;
}

MY_TEST(RtcpReceiverTest, BrokenPacketIsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));

    const uint8_t bad_packet[] = {0, 0, 0, 0};
    EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated).Times(0);
    receiver.IncomingRtcpPacket(bad_packet);
}

MY_TEST(RtcpReceiverTest, InvalidFeedbackIsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));

    // Too short feedback packet.
    const uint8_t bad_packet[] = {0x81 /*nack*/, rtcp::Rtpfb::kPacketType, 0, 0};
    // EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated).Times(0);
    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(kReceiverMainSsrc, 
                                              Field(&RtcpPacketTypeCounter::pli_packets, 0)));
    EXPECT_CALL(mocks.transport_feedback_observer, OnTransportFeedback).Times(0);
    receiver.IncomingRtcpPacket(bad_packet);
}

MY_TEST(RtcpReceiverTest, InjectSrPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    EXPECT_FALSE(receiver.GetLastSenderReportStats().has_value());

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(IsEmpty(), _));
    receiver.IncomingRtcpPacket(sr.Build());

    EXPECT_TRUE(receiver.GetLastSenderReportStats().has_value());
}

MY_TEST(RtcpReceiverTest, InjectSrPacketFromUnknownSender) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kUnknownSenderSsrc);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(_, _));
    receiver.IncomingRtcpPacket(sr.Build());

    EXPECT_FALSE(receiver.GetLastSenderReportStats().has_value());
}

MY_TEST(RtcpReceiverTest, InjectSrPacketCalculatesRTT) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const int64_t kRttMs = 123;
    const uint32_t kDelayNtp = 0x4321;
    const int64_t kDelayMs = CompactNtpRttToMs(kDelayNtp);

    auto rtt_stats = receiver.GetRttStats(kSenderSsrc);
    EXPECT_FALSE(rtt_stats.has_value());

    uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
    mocks.clock.AdvanceTimeMs(kRttMs + kDelayMs);

    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock block;
    block.set_media_ssrc(kReceiverMainSsrc);
    block.set_last_sr_ntp_timestamp(sent_ntp);
    block.set_delay_sr_since_last_sr(kDelayNtp);
    sr.AddReportBlock(block);

    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(sr.Build());

    rtt_stats = receiver.GetRttStats(kSenderSsrc);
    EXPECT_TRUE(rtt_stats.has_value());
    EXPECT_NEAR(kRttMs, rtt_stats->last_rtt().ms(), 1);

}

MY_TEST(RtcpReceiverTest, InjectSrPacketCalculatesNegativeRTTAsOne) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const int64_t kRttMs = -13;
    const uint32_t kDelayNtp = 0x4321;
    const int64_t kDelayMs = CompactNtpRttToMs(kDelayNtp);

    auto rtt_stats = receiver.GetRttStats(kSenderSsrc);
    EXPECT_FALSE(rtt_stats.has_value());

    uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
    mocks.clock.AdvanceTimeMs(kRttMs + kDelayMs);

    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock block;
    block.set_media_ssrc(kReceiverMainSsrc);
    block.set_last_sr_ntp_timestamp(sent_ntp);
    block.set_delay_sr_since_last_sr(kDelayNtp);
    sr.AddReportBlock(block);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(1), _));
    receiver.IncomingRtcpPacket(sr.Build());

    rtt_stats = receiver.GetRttStats(kSenderSsrc);
    EXPECT_TRUE(rtt_stats.has_value());
    EXPECT_EQ(1, rtt_stats->last_rtt().ms());
}

MY_TEST(RtcpReceiverTest, TwoReportBlocksWithLastOneWithoutLastSrCalculatesRttForBandwidthObserver) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
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
    
    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(2), kRttMs));
    receiver.IncomingRtcpPacket(sr.Build());
}

MY_TEST(RtcpReceiverTest, InjectRrPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(IsEmpty(), _));
    receiver.IncomingRtcpPacket(rr.Build());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), IsEmpty());
}

MY_TEST(RtcpReceiverTest, InjectRrPacketWithReportBlockNotToUsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kNotToUsSsrc);
    rr.AddReportBlock(rb);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(IsEmpty(), _));
    receiver.IncomingRtcpPacket(rr.Build());

    EXPECT_EQ(0, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), IsEmpty());
}

MY_TEST(RtcpReceiverTest, InjectRrPacketWithOneReportBlock) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kReceiverMainSsrc);
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rr.AddReportBlock(rb);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(1), _));

    receiver.IncomingRtcpPacket(rr.Build());
    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(1));
}

MY_TEST(RtcpReceiverTest, InjectSrPacketWithOneReportBlock) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kReceiverMainSsrc);
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    sr.AddReportBlock(rb);

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(1), _));

    receiver.IncomingRtcpPacket(sr.Build());
    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(1));
}

MY_TEST(RtcpReceiverTest, InjectRrPacketWithTwoReportBlocks) {
    const uint16_t kSequenceNumbers[] = {10, 12423};
    const uint32_t kCumLost[] = {13, 555};
    const uint8_t kFracLost[] = {20, 11};
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
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

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(2), _));
    receiver.IncomingRtcpPacket(rr1.Build());
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

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(2), _));
    receiver.IncomingRtcpPacket(rr2.Build());
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
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
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
    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(1), _));
    receiver.IncomingRtcpPacket(rr1.Build());

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

    EXPECT_CALL(mocks.report_blocks_observer,
                OnReceivedRtcpReportBlocks(SizeIs(1), _));
    receiver.IncomingRtcpPacket(rr2.Build());

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
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    // No report block received.
    auto rtt_stats = receiver.GetRttStats(kSenderSsrc);
    EXPECT_FALSE(rtt_stats.has_value());

    rtcp::ReportBlock rb;
    rb.set_media_ssrc(kReceiverMainSsrc);
    rb.set_last_sr_ntp_timestamp(kSentCompactNtp);
    rb.set_delay_sr_since_last_sr(kDelayCompactNtp);
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rr.AddReportBlock(rb);
    int64_t now_ms = mocks.clock.now_ms();

    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr.Build());

    EXPECT_EQ(now_ms, receiver.LastReceivedReportBlockMs());
    EXPECT_TRUE(receiver.GetRttStats(kSenderSsrc).has_value());
}

MY_TEST(RtcpReceiverTest, InjectSdesWithOneChunk) {
    ReceiverMocks mocks;
    MockCnameObserver cname_observer;
    RtcpConfiguration config = DefaultConfiguration(&mocks);
    config.cname_observer = &cname_observer;

    RtcpReceiver receiver(config);
    receiver.set_remote_ssrc(kSenderSsrc);

    const char kCname[] = "alice@host";
    rtcp::Sdes sdes;
    sdes.AddCName(kSenderSsrc, kCname);

    EXPECT_CALL(cname_observer, OnCname(kSenderSsrc, StrEq(kCname)));
    receiver.IncomingRtcpPacket(sdes.Build());
}

MY_TEST(RtcpReceiverTest, InjectByePacket_RemovesReportBlocks) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::ReportBlock rb1;
    rb1.set_media_ssrc(kReceiverMainSsrc);
    rtcp::ReportBlock rb2;
    rb2.set_media_ssrc(kReceiverExtraSsrc);
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    rr.AddReportBlock(rb1);
    rr.AddReportBlock(rb2);

    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr.Build());

    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(2));
    // Verify that BYE removes the report blocks.
    rtcp::Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);
    receiver.IncomingRtcpPacket(bye.Build());

    EXPECT_THAT(receiver.GetLatestReportBlocks(), IsEmpty());
    // Inject packet again.
    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr.Build());

    EXPECT_THAT(receiver.GetLatestReportBlocks(), SizeIs(2));
}

MY_TEST(RtcpReceiverTest, InjectPliPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::Pli pli;
    pli.set_media_ssrc(kReceiverMainSsrc);

    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(kReceiverMainSsrc, 
                                              Field(&RtcpPacketTypeCounter::pli_packets, 1)));

    EXPECT_CALL(mocks.intra_frame_observer,
                OnReceivedIntraFrameRequest(kReceiverMainSsrc));
    receiver.IncomingRtcpPacket(pli.Build());
}

MY_TEST(RtcpReceiverTest, PliPacketNotToUsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::Pli pli;
    pli.set_media_ssrc(kNotToUsSsrc);

    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(kReceiverMainSsrc, 
                                              Field(&RtcpPacketTypeCounter::pli_packets, 0)));

    EXPECT_CALL(mocks.intra_frame_observer, OnReceivedIntraFrameRequest).Times(0);
    receiver.IncomingRtcpPacket(pli.Build());
}

MY_TEST(RtcpReceiverTest, InjectFirPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::Fir fir;
    fir.AddRequestTo(kReceiverMainSsrc, 13);

    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(kReceiverMainSsrc, Field(&RtcpPacketTypeCounter::fir_packets, 1)));
                
    EXPECT_CALL(mocks.intra_frame_observer,
                OnReceivedIntraFrameRequest(kReceiverMainSsrc));
    receiver.IncomingRtcpPacket(fir.Build());
}

MY_TEST(RtcpReceiverTest, FirPacketNotToUsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::Fir fir;
    fir.AddRequestTo(kNotToUsSsrc, 13);

    EXPECT_CALL(mocks.intra_frame_observer, OnReceivedIntraFrameRequest).Times(0);
    receiver.IncomingRtcpPacket(fir.Build());
}

MY_TEST(RtcpReceiverTest, ReceiveReportTimeout) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const uint16_t kSequenceNumber = 1234;
    mocks.clock.AdvanceTimeMs(3 * kRtcpIntervalMs);

    // No RR received, shouldn't trigger a timeout.
    EXPECT_FALSE(receiver.RtcpRrTimeout());
    EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

    // Add a RR and advance the clock just enough to not trigger a timeout.
    rtcp::ReportBlock rb1;
    rb1.set_media_ssrc(kReceiverMainSsrc);
    rb1.set_extended_highest_sequence_num(kSequenceNumber);
    rtcp::ReceiverReport rr1;
    rr1.set_sender_ssrc(kSenderSsrc);
    rr1.AddReportBlock(rb1);

    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr1.Build());

    mocks.clock.AdvanceTimeMs(3 * kRtcpIntervalMs - 1);
    EXPECT_FALSE(receiver.RtcpRrTimeout());
    EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

    // Add a RR with the same extended max as the previous RR to trigger a
    // sequence number timeout, but not a RR timeout.
    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr1.Build());

    mocks.clock.AdvanceTimeMs(2);
    EXPECT_FALSE(receiver.RtcpRrTimeout());
    EXPECT_TRUE(receiver.RtcpRrSequenceNumberTimeout());

    // Advance clock enough to trigger an RR timeout too.
    mocks.clock.AdvanceTimeMs(3 * kRtcpIntervalMs);
    EXPECT_TRUE(receiver.RtcpRrTimeout());

    // We should only get one timeout even though we still haven't received a new
    // RR.
    EXPECT_FALSE(receiver.RtcpRrTimeout());
    EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

    // Add a new RR with increase sequence number to reset timers.
    rtcp::ReportBlock rb2;
    rb2.set_media_ssrc(kReceiverMainSsrc);
    rb2.set_extended_highest_sequence_num(kSequenceNumber + 1);
    rtcp::ReceiverReport rr2;
    rr2.set_sender_ssrc(kSenderSsrc);
    rr2.AddReportBlock(rb2);

    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr2.Build());

    EXPECT_FALSE(receiver.RtcpRrTimeout());
    EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

    // Verify we can get a timeout again once we've received new RR.
    mocks.clock.AdvanceTimeMs(2 * kRtcpIntervalMs);
    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingRtcpPacket(rr2.Build());

    mocks.clock.AdvanceTimeMs(kRtcpIntervalMs + 1);
    EXPECT_FALSE(receiver.RtcpRrTimeout());
    EXPECT_TRUE(receiver.RtcpRrSequenceNumberTimeout());

    mocks.clock.AdvanceTimeMs(2 * kRtcpIntervalMs);
    EXPECT_TRUE(receiver.RtcpRrTimeout());
}

MY_TEST(RtcpReceiverTest, VerifyBlockAndTimestampObtainedFromReportBlockDataObserver) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const uint8_t kFractionLoss = 3;
    const uint32_t kCumulativeLoss = 7;
    const uint32_t kJitter = 9;
    const uint16_t kSequenceNumber = 1234;
    const int64_t kUtcNowUs = 42;

    rtcp::ReportBlock rtcp_block;
    rtcp_block.set_media_ssrc(kReceiverMainSsrc);
    rtcp_block.set_extended_highest_sequence_num(kSequenceNumber);
    rtcp_block.set_fraction_lost(kFractionLoss);
    rtcp_block.set_cumulative_packet_lost(kCumulativeLoss);
    rtcp_block.set_jitter(kJitter);

    rtcp::ReceiverReport rtcp_report;
    rtcp_report.set_sender_ssrc(kSenderSsrc);
    rtcp_report.AddReportBlock(rtcp_block);
    EXPECT_CALL(mocks.report_blocks_observer, OnReceivedRtcpReportBlocks)
        .WillOnce([&](const std::vector<RtcpReportBlock>& report_blocks, int64_t rtt_ms) {
            const auto& report_block = report_blocks[0];
            EXPECT_EQ(rtcp_block.source_ssrc(), report_block.source_ssrc);
            EXPECT_EQ(kSenderSsrc, report_block.sender_ssrc);
            EXPECT_EQ(rtcp_block.fraction_lost(), report_block.fraction_lost);
            EXPECT_EQ(rtcp_block.cumulative_packet_lost(),
                     report_block.packets_lost);
            EXPECT_EQ(rtcp_block.extended_high_seq_num(),
                     report_block.extended_highest_sequence_number);
            EXPECT_EQ(rtcp_block.jitter(), report_block.jitter);
        });
    receiver.IncomingRtcpPacket(rtcp_report.Build());
}

MY_TEST(RtcpReceiverTest, ReceivesTransportFeedback) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    rtcp::TransportFeedback packet;
    packet.set_media_ssrc(kReceiverMainSsrc);
    packet.set_sender_ssrc(kSenderSsrc);
    packet.SetBase(1, 1000);
    packet.AddReceivedPacket(1, 1000);

    EXPECT_CALL(
        mocks.transport_feedback_observer,
        OnTransportFeedback(AllOf(
            Property(&rtcp::TransportFeedback::media_ssrc, kReceiverMainSsrc),
            Property(&rtcp::TransportFeedback::sender_ssrc, kSenderSsrc))));
    receiver.IncomingRtcpPacket(packet.Build());
}

MY_TEST(RtcpReceiverTest, ReceivesRemb) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const uint32_t kBitrateBps = 500000;
    rtcp::Remb remb;
    remb.set_sender_ssrc(kSenderSsrc);
    remb.set_bitrate_bps(kBitrateBps);

    EXPECT_CALL(mocks.bandwidth_observer,
                OnReceivedEstimatedBitrateBps(kBitrateBps));
    receiver.IncomingRtcpPacket(remb.Build());
}

MY_TEST(RtcpReceiverTest, Nack) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const uint16_t kNackList1[] = {1, 2, 3, 5};
    const uint16_t kNackList23[] = {5, 7, 30, 40, 41, 58, 59, 61, 63};
    const size_t kNackListLength2 = 4;
    const size_t kNackListLength3 = arraysize(kNackList23) - kNackListLength2;
    std::set<uint16_t> nack_set;
    nack_set.insert(std::begin(kNackList1), std::end(kNackList1));
    nack_set.insert(std::begin(kNackList23), std::end(kNackList23));

    auto nack1 = std::make_unique<rtcp::Nack>();
    nack1->set_sender_ssrc(kSenderSsrc);
    nack1->set_media_ssrc(kReceiverMainSsrc);
    nack1->set_packet_ids(kNackList1, arraysize(kNackList1));

    EXPECT_CALL(mocks.nack_list_observer,
                OnReceivedNack(ElementsAreArray(kNackList1), _));
    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(
                    kReceiverMainSsrc,
                    AllOf(Field(&RtcpPacketTypeCounter::nack_requests,
                                arraysize(kNackList1)),
                            Field(&RtcpPacketTypeCounter::unique_nack_requests,
                                arraysize(kNackList1)))));
    receiver.IncomingRtcpPacket(nack1->Build());

    auto nack2 = std::make_unique<rtcp::Nack>();
    nack2->set_sender_ssrc(kSenderSsrc);
    nack2->set_media_ssrc(kReceiverMainSsrc);
    nack2->set_packet_ids(kNackList23, kNackListLength2);

    auto nack3 = std::make_unique<rtcp::Nack>();
    nack3->set_sender_ssrc(kSenderSsrc);
    nack3->set_media_ssrc(kReceiverMainSsrc);
    nack3->set_packet_ids(kNackList23 + kNackListLength2, kNackListLength3);

    rtcp::CompoundPacket two_nacks;
    two_nacks.Append(std::move(nack2));
    two_nacks.Append(std::move(nack3));

    EXPECT_CALL(mocks.nack_list_observer,
                OnReceivedNack(ElementsAreArray(kNackList23), _));
    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(
                    kReceiverMainSsrc,
                    AllOf(Field(&RtcpPacketTypeCounter::nack_requests,
                                arraysize(kNackList1) + arraysize(kNackList23)),
                            Field(&RtcpPacketTypeCounter::unique_nack_requests,
                                nack_set.size()))));
    receiver.IncomingRtcpPacket(two_nacks.Build());
}

MY_TEST(RtcpReceiverTest, NackNotForUsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks));
    receiver.set_remote_ssrc(kSenderSsrc);

    const uint16_t kNackList1[] = {1, 2, 3, 5};
    const size_t kNackListLength1 = std::end(kNackList1) - std::begin(kNackList1);

    rtcp::Nack nack;
    nack.set_sender_ssrc(kSenderSsrc);
    nack.set_media_ssrc(kNotToUsSsrc);
    nack.set_packet_ids(kNackList1, kNackListLength1);

    EXPECT_CALL(mocks.packet_type_counter_observer,
                RtcpPacketTypesCounterUpdated(
                    _, Field(&RtcpPacketTypeCounter::nack_requests, 0)));
    receiver.IncomingRtcpPacket(nack.Build());
}
    
} // namespace test
} // namespace naivertc

