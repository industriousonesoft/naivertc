#include "rtc/rtp_rtcp/components/rtp_receive_statistics.hpp"
#include "testing/simulated_clock.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {
    
constexpr size_t kPacketSize1 = 100;
constexpr size_t kPacketSize2 = 300;
constexpr uint32_t kSsrc1 = 101;
constexpr uint32_t kSsrc2 = 202;
constexpr uint32_t kSsrc3 = 203;
constexpr uint32_t kSsrc4 = 304;

RtpPacketReceived CreateRtpPacket(uint32_t ssrc, 
                                  size_t header_size, 
                                  size_t payload_size, 
                                  size_t padding_size) {
    RtpPacketReceived packet_received;
    packet_received.set_ssrc(ssrc);
    packet_received.set_sequence_number(100);
    packet_received.set_payload_type_frequency(90000);
    EXPECT_GE(header_size, 12);
    EXPECT_EQ(header_size % 4, 0);

    if (header_size > 12) {
        const int num_csrcs = (header_size - 12) / 4;
        std::vector<uint32_t> csrcs(num_csrcs);
        packet_received.set_csrcs(csrcs);
    }
    packet_received.SetPayloadSize(payload_size);
    packet_received.SetPadding(padding_size);
    return packet_received;
}

RtpPacketReceived CreateRtpPacket(uint32_t ssrc, size_t packet_size) {
    return CreateRtpPacket(ssrc, 12, packet_size - 12, 0);
}

void IncrementSeqNum(RtpPacketReceived* packet, uint16_t incr) {
    packet->set_sequence_number(packet->sequence_number() + incr);
}

void IncrementSeqNum(RtpPacketReceived* packet) {
    IncrementSeqNum(packet, 1);
}

} // namespace

class T(RtpReceiveStatisticsTest) : public ::testing::Test {
public:
    T(RtpReceiveStatisticsTest)() 
        : clock_(0),
          receive_statistics_(&clock_) {
        packet1_ = CreateRtpPacket(kSsrc1, kPacketSize1);
        packet2_ = CreateRtpPacket(kSsrc2, kPacketSize2);
    }

protected:
    SimulatedClock clock_;
    RtpReceiveStatistics receive_statistics_;
    RtpPacketReceived packet1_;
    RtpPacketReceived packet2_;
};

MY_TEST_F(RtpReceiveStatisticsTest, TwoIncomingSsrcs) {
    receive_statistics_.OnRtpPacket(packet1_);
    IncrementSeqNum(&packet1_);
    receive_statistics_.OnRtpPacket(packet2_);
    IncrementSeqNum(&packet2_);
    clock_.AdvanceTimeMs(100);
    receive_statistics_.OnRtpPacket(packet1_);
    IncrementSeqNum(&packet1_);
    receive_statistics_.OnRtpPacket(packet2_);
    IncrementSeqNum(&packet2_);

    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    ASSERT_TRUE(statistician != nullptr);
    EXPECT_TRUE(statistician->GetReceivedBitrate().has_value());
    EXPECT_GT(statistician->GetReceivedBitrate().value().bps(), 0u);
    RtpStreamDataCounters counters = statistician->GetReceiveStreamDataCounters();
    EXPECT_EQ(176u, counters.transmitted.payload_bytes);
    EXPECT_EQ(24u, counters.transmitted.header_bytes);
    EXPECT_EQ(0u, counters.transmitted.padding_bytes);
    EXPECT_EQ(2u, counters.transmitted.num_packets);

    statistician = receive_statistics_.GetStatistician(kSsrc2);
    ASSERT_TRUE(statistician != nullptr);
    EXPECT_TRUE(statistician->GetReceivedBitrate().has_value());
    EXPECT_GT(statistician->GetReceivedBitrate().value().bps(), 0u);
    counters = statistician->GetReceiveStreamDataCounters();
    EXPECT_EQ(576u, counters.transmitted.payload_bytes);
    EXPECT_EQ(24u, counters.transmitted.header_bytes);
    EXPECT_EQ(0u, counters.transmitted.padding_bytes);
    EXPECT_EQ(2u, counters.transmitted.num_packets);

    EXPECT_EQ(2u, receive_statistics_.GetRtcpReportBlocks(3).size());

    // Add more incoming packets and verify that they are registered
    // in both access methods.
    receive_statistics_.OnRtpPacket(packet1_);
    IncrementSeqNum(&packet1_);
    receive_statistics_.OnRtpPacket(packet2_);
    IncrementSeqNum(&packet2_);

    counters = receive_statistics_.GetStatistician(kSsrc1)->GetReceiveStreamDataCounters();
    EXPECT_EQ(264u, counters.transmitted.payload_bytes);
    EXPECT_EQ(36u, counters.transmitted.header_bytes);
    EXPECT_EQ(0u, counters.transmitted.padding_bytes);
    EXPECT_EQ(3u, counters.transmitted.num_packets);

    counters = receive_statistics_.GetStatistician(kSsrc2)->GetReceiveStreamDataCounters();
    EXPECT_EQ(864u, counters.transmitted.payload_bytes);
    EXPECT_EQ(36u, counters.transmitted.header_bytes);
    EXPECT_EQ(0u, counters.transmitted.padding_bytes);
    EXPECT_EQ(3u, counters.transmitted.num_packets);
}

MY_TEST_F(RtpReceiveStatisticsTest, RtcpReportBlocksReturnsMaxBlocksWhenThereAreMoreStatisticians) {
    RtpPacketReceived packet1 = CreateRtpPacket(kSsrc1, kPacketSize1);
    RtpPacketReceived packet2 = CreateRtpPacket(kSsrc2, kPacketSize1);
    RtpPacketReceived packet3 = CreateRtpPacket(kSsrc3, kPacketSize1);
    receive_statistics_.OnRtpPacket(packet1);
    receive_statistics_.OnRtpPacket(packet2);
    receive_statistics_.OnRtpPacket(packet3);

    EXPECT_THAT(receive_statistics_.GetRtcpReportBlocks(2), ::testing::SizeIs(2));
    EXPECT_THAT(receive_statistics_.GetRtcpReportBlocks(2), ::testing::SizeIs(2));
    EXPECT_THAT(receive_statistics_.GetRtcpReportBlocks(2), ::testing::SizeIs(2));
}

MY_TEST_F(RtpReceiveStatisticsTest, RtcpReportBlocksReturnsAllObservedSsrcsWithMultipleCalls) {
    RtpPacketReceived packet1 = CreateRtpPacket(kSsrc1, kPacketSize1);
    RtpPacketReceived packet2 = CreateRtpPacket(kSsrc2, kPacketSize1);
    RtpPacketReceived packet3 = CreateRtpPacket(kSsrc3, kPacketSize1);
    RtpPacketReceived packet4 = CreateRtpPacket(kSsrc4, kPacketSize1);
    receive_statistics_.OnRtpPacket(packet1);
    receive_statistics_.OnRtpPacket(packet2);
    receive_statistics_.OnRtpPacket(packet3);
    receive_statistics_.OnRtpPacket(packet4);

    std::vector<uint32_t> observed_ssrcs;
    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(2);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(2));
    observed_ssrcs.push_back(report_blocks[0].source_ssrc());
    observed_ssrcs.push_back(report_blocks[1].source_ssrc());

    report_blocks = receive_statistics_.GetRtcpReportBlocks(2);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(2));
    observed_ssrcs.push_back(report_blocks[0].source_ssrc());
    observed_ssrcs.push_back(report_blocks[1].source_ssrc());

    EXPECT_THAT(observed_ssrcs, ::testing::UnorderedElementsAre(kSsrc1, kSsrc2, kSsrc3, kSsrc4));
}

MY_TEST_F(RtpReceiveStatisticsTest, ActiveStatisticians) {
    receive_statistics_.OnRtpPacket(packet1_);
    IncrementSeqNum(&packet1_);
    clock_.AdvanceTimeMs(1000);
    receive_statistics_.OnRtpPacket(packet2_);
    IncrementSeqNum(&packet2_);
    // Nothing should time out since only 1000 ms has passed since the first
    // packet came in.
    EXPECT_EQ(2u, receive_statistics_.GetRtcpReportBlocks(3).size());

    clock_.AdvanceTimeMs(7000);
    // kSsrc1 should have timed out (8s).
    EXPECT_EQ(1u, receive_statistics_.GetRtcpReportBlocks(3).size());

    clock_.AdvanceTimeMs(1000);
    // kSsrc2 should have timed out.
    EXPECT_EQ(0u, receive_statistics_.GetRtcpReportBlocks(3).size());

    receive_statistics_.OnRtpPacket(packet1_);
    IncrementSeqNum(&packet1_);
    // kSsrc1 should be active again and the data counters should have survived.
    EXPECT_EQ(1u, receive_statistics_.GetRtcpReportBlocks(3).size());
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    ASSERT_TRUE(statistician != NULL);
    RtpStreamDataCounters counters = statistician->GetReceiveStreamDataCounters();
    EXPECT_EQ(176u, counters.transmitted.payload_bytes);
    EXPECT_EQ(24u, counters.transmitted.header_bytes);
    EXPECT_EQ(0u, counters.transmitted.padding_bytes);
    EXPECT_EQ(2u, counters.transmitted.num_packets);
}

MY_TEST_F(RtpReceiveStatisticsTest, DoesntCreateRtcpReportBlockUntilFirstReceivedPacketForSsrc) {
    // Creates a statistician object for the ssrc.
    receive_statistics_.EnableRetransmitDetection(kSsrc1, true);
    EXPECT_TRUE(receive_statistics_.GetStatistician(kSsrc1) != nullptr);
    EXPECT_EQ(0u, receive_statistics_.GetRtcpReportBlocks(3).size());
    // Receive first packet
    receive_statistics_.OnRtpPacket(packet1_);
    EXPECT_EQ(1u, receive_statistics_.GetRtcpReportBlocks(3).size());
}

MY_TEST_F(RtpReceiveStatisticsTest, GetReceiveStreamDataCounters) {
    receive_statistics_.OnRtpPacket(packet1_);
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    ASSERT_TRUE(statistician != NULL);

    RtpStreamDataCounters counters = statistician->GetReceiveStreamDataCounters();
    EXPECT_GT(counters.first_packet_time_ms, -1);
    EXPECT_EQ(1u, counters.transmitted.num_packets);

    receive_statistics_.OnRtpPacket(packet1_);
    counters = statistician->GetReceiveStreamDataCounters();
    EXPECT_GT(counters.first_packet_time_ms, -1);
    EXPECT_EQ(2u, counters.transmitted.num_packets);
}

MY_TEST_F(RtpReceiveStatisticsTest, SimpleLossComputation) {
    packet1_.set_sequence_number(1);
    receive_statistics_.OnRtpPacket(packet1_);
    // Lost the second packet (seq_num = 2).
    packet1_.set_sequence_number(3);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(4);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(5);
    receive_statistics_.OnRtpPacket(packet1_);

    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    // 20% = 51/255.
    EXPECT_EQ(51u, report_blocks[0].fraction_lost());
    EXPECT_EQ(1, report_blocks[0].cumulative_packet_lost());
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    EXPECT_EQ(20, statistician->GetFractionLostInPercent());
}

MY_TEST_F(RtpReceiveStatisticsTest, LossComputationWithReordering) {
    packet1_.set_sequence_number(1);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(3);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(2);
    receive_statistics_.OnRtpPacket(packet1_);
    // The fourth packet was lost.
    packet1_.set_sequence_number(5);
    receive_statistics_.OnRtpPacket(packet1_);

    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    // 20% = 51/255.
    EXPECT_EQ(51u, report_blocks[0].fraction_lost());
    EXPECT_EQ(1, report_blocks[0].cumulative_packet_lost());
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    EXPECT_EQ(20, statistician->GetFractionLostInPercent());
}

MY_TEST_F(RtpReceiveStatisticsTest, LossComputationWithDuplicates) {
    // Lose 2 packets, but also receive 1 duplicate. Should actually count as
    // only 1 packet being lost.
    packet1_.set_sequence_number(1);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(4);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(4);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(5);
    receive_statistics_.OnRtpPacket(packet1_);

    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    // 20% = 51/255.
    EXPECT_EQ(51u, report_blocks[0].fraction_lost());
    EXPECT_EQ(1, report_blocks[0].cumulative_packet_lost());
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    EXPECT_EQ(20, statistician->GetFractionLostInPercent());
}

MY_TEST_F(RtpReceiveStatisticsTest, LossComputationWithSequenceNumberWrapping) {
    // First, test loss computation over a period that included a sequence number
    // rollover.
    packet1_.set_sequence_number(0xfffd);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(0);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(0xfffe);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(1);
    receive_statistics_.OnRtpPacket(packet1_);

    // Only one packet was actually lost, 0xffff.
    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    // 20% = 51/255.
    EXPECT_EQ(51u, report_blocks[0].fraction_lost());
    EXPECT_EQ(1, report_blocks[0].cumulative_packet_lost());
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    EXPECT_EQ(20, statistician->GetFractionLostInPercent());

    // Now test losing one packet *after* the rollover.
    packet1_.set_sequence_number(3);
    receive_statistics_.OnRtpPacket(packet1_);

    report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    // 50% = 127/255.
    EXPECT_EQ(127u, report_blocks[0].fraction_lost());
    EXPECT_EQ(2, report_blocks[0].cumulative_packet_lost());
    // 2 packets lost, 7 expected
    EXPECT_EQ(28, statistician->GetFractionLostInPercent());
}

MY_TEST_F(RtpReceiveStatisticsTest, StreamRestartDoesntCountAsLoss) {
    receive_statistics_.SetMaxReorderingThreshold(kSsrc1, 200);

    packet1_.set_sequence_number(0);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(1);
    receive_statistics_.OnRtpPacket(packet1_);

    // Indicates a stream restart.
    packet1_.set_sequence_number(400);
    receive_statistics_.OnRtpPacket(packet1_);

    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    EXPECT_EQ(0, report_blocks[0].fraction_lost());
    EXPECT_EQ(0, report_blocks[0].cumulative_packet_lost());
    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    EXPECT_EQ(0, statistician->GetFractionLostInPercent());

    packet1_.set_sequence_number(401);
    receive_statistics_.OnRtpPacket(packet1_);
    report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    EXPECT_EQ(0, report_blocks[0].fraction_lost());
    EXPECT_EQ(0, report_blocks[0].cumulative_packet_lost());
    EXPECT_EQ(0, statistician->GetFractionLostInPercent());
}

MY_TEST_F(RtpReceiveStatisticsTest, CountsLossAfterStreamRestart) {
    receive_statistics_.SetMaxReorderingThreshold(kSsrc1, 200);

    packet1_.set_sequence_number(0);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(1);
    receive_statistics_.OnRtpPacket(packet1_);

    // A stream restart.
    packet1_.set_sequence_number(400);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(401);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(403);
    receive_statistics_.OnRtpPacket(packet1_);

    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    // 25% ~= 63/255.
    EXPECT_EQ(63u, report_blocks[0].fraction_lost());
    EXPECT_EQ(1, report_blocks[0].cumulative_packet_lost());

    RtpStreamStatistician* statistician = receive_statistics_.GetStatistician(kSsrc1);
    // Actual value: 1 / 404 ~= 0.00248 = 0.248%
    EXPECT_EQ(0, statistician->GetFractionLostInPercent());
}

MY_TEST_F(RtpReceiveStatisticsTest, StreamCanRestartAtSequenceNumberWrapAround) {
    receive_statistics_.SetMaxReorderingThreshold(kSsrc1, 200);

    packet1_.set_sequence_number(0xffff - 401);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(0xffff - 400);
    receive_statistics_.OnRtpPacket(packet1_);

    // A stream restarted.
    packet1_.set_sequence_number(0xffff);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(0);
    receive_statistics_.OnRtpPacket(packet1_);
    packet1_.set_sequence_number(2);
    receive_statistics_.OnRtpPacket(packet1_);

    std::vector<rtcp::ReportBlock> report_blocks = receive_statistics_.GetRtcpReportBlocks(1);
    ASSERT_THAT(report_blocks, ::testing::SizeIs(1));
    EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

    EXPECT_EQ(1, report_blocks[0].cumulative_packet_lost());
}

    
} // namespace test
} // namespace naivertc
