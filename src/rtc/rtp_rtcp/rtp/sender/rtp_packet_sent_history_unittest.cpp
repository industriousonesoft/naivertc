#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

namespace naivertc {
namespace test {
namespace {

// Initialize with a high sequence number
// so we'll encounter a wrap-around.
const uint16_t kStartSeqNum = 65534;

uint16_t Unwrap(size_t seq_num) {
    return static_cast<uint16_t>(seq_num & 0xFFFF);
}

} // namespace

using StorageMode = RtpPacketSentHistory::StorageMode;

// RtpPacketSentHistoryTest
class T(RtpPacketSentHistoryTest) : public ::testing::TestWithParam<bool> {
protected:
    T(RtpPacketSentHistoryTest)() 
        : clock_(123456),
          packet_history_(&clock_, /*enable_padding_prio=*/GetParam()) {}

    RtpPacketToSend CreateRtpPacket(uint16_t seq_num, int64_t capture_time_ms = 0) {
        RtpPacketToSend packet(nullptr);
        packet.set_sequence_number(seq_num);
        packet.set_capture_time_ms(capture_time_ms);
        packet.set_allow_retransmission(true);
        return packet;
    }

protected:
    SimulatedClock clock_;
    RtpPacketSentHistory packet_history_;
};

MY_INSTANTIATE_TEST_SUITE_P(WithAndWithoutPaddingPrio, RtpPacketSentHistoryTest, ::testing::Bool());

MY_TEST_P(RtpPacketSentHistoryTest, SetStorageStatus) {
    EXPECT_EQ(StorageMode::DISABLE, packet_history_.GetStorageMode());
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_EQ(StorageMode::STORE_AND_CULL, packet_history_.GetStorageMode());
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_EQ(StorageMode::STORE_AND_CULL, packet_history_.GetStorageMode());
    packet_history_.SetStorePacketsStatus(StorageMode::DISABLE, 0);
    EXPECT_EQ(StorageMode::DISABLE, packet_history_.GetStorageMode());
}

MY_TEST_P(RtpPacketSentHistoryTest, ClearHistoryAfterSetStorageStatus) {
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 0);
    packet_history_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));
    EXPECT_TRUE(packet_history_.GetPacketState(kStartSeqNum));

    // Changing storage status, even to the current one, will clear the history.
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_history_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, StartSeqNumResetAfterReset) {
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    uint16_t seq_num = kStartSeqNum;
    packet_history_.PutRtpPacket(CreateRtpPacket(seq_num));
    EXPECT_TRUE(packet_history_.GetPacketState(seq_num));

    // Changing store status, to clear the history.
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_history_.GetPacketState(seq_num));

    // Add a new packet
    seq_num = Unwrap(seq_num + 1);
    packet_history_.PutRtpPacket(CreateRtpPacket(seq_num));
    EXPECT_TRUE(packet_history_.GetPacketState(seq_num));

    // Advance time past where packet expires.
    clock_.AdvanceTimeMs(RtpPacketSentHistory::kPacketCullingDelayFactor * RtpPacketSentHistory::kMinPacketDurationMs);

    seq_num = Unwrap(seq_num + 1);
    packet_history_.PutRtpPacket(CreateRtpPacket(seq_num));
    EXPECT_FALSE(packet_history_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_history_.GetPacketState(Unwrap(kStartSeqNum + 1)));
    EXPECT_TRUE(packet_history_.GetPacketState(Unwrap(kStartSeqNum + 2)));
}

MY_TEST_P(RtpPacketSentHistoryTest, NoStoreStatus) {
    EXPECT_EQ(StorageMode::DISABLE, packet_history_.GetStorageMode());
    packet_history_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));
    // Packet should not be stored.
    EXPECT_FALSE(packet_history_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, GetRtpPacketNotStored) {
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_history_.GetPacketState(0));
}

MY_TEST_P(RtpPacketSentHistoryTest, PutRtpPacket) {
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_history_.GetPacketState(kStartSeqNum));
    packet_history_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));
    EXPECT_TRUE(packet_history_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, GetRtpPacket) {
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 0);
    int64_t capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    auto packet_in = packet;

    packet_history_.PutRtpPacket(std::move(packet));
    auto packet_out = packet_history_.GetPacketAndSetSendTime(kStartSeqNum);
    EXPECT_TRUE(packet_out.has_value());
    EXPECT_EQ(packet_in, *packet_out);
    EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

MY_TEST_P(RtpPacketSentHistoryTest, PacketStateIsCorrect) {
    const uint32_t kSsrc = 9876543;
    const int64_t kRttMs = 100;
    
    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    packet_history_.SetRttMs(kRttMs);

    int64_t capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    packet.set_ssrc(kSsrc);
    packet.set_payload_type(98);
    const size_t packet_size = packet.size();

    clock_.AdvanceTimeMs(100);
    int64_t send_time_ms = clock_.now_ms();
    packet_history_.PutRtpPacket(std::move(packet), send_time_ms);

    auto state = packet_history_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->rtp_sequence_number, kStartSeqNum);
    EXPECT_EQ(state->send_time_ms, send_time_ms);
    EXPECT_EQ(state->capture_time_ms, capture_time_ms);
    EXPECT_EQ(state->ssrc, kSsrc);
    EXPECT_EQ(state->packet_size, packet_size);
    EXPECT_EQ(state->num_retransmitted, 0);

    clock_.AdvanceTimeMs(1);
    EXPECT_TRUE(packet_history_.GetPacketAndSetSendTime(kStartSeqNum));
    clock_.AdvanceTimeMs(kRttMs + 1);

    state = packet_history_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->num_retransmitted, 1);
}

MY_TEST_P(RtpPacketSentHistoryTest, MinResendTimeWithPacer) {
    static const int64_t kMinRetransmitIntervalMs = 100;

    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    packet_history_.SetRttMs(kMinRetransmitIntervalMs);
    int capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    size_t packet_size = packet.size();
    packet_history_.PutRtpPacket(std::move(packet));

    // First transmission call from pacer.
    EXPECT_TRUE(packet_history_.GetPacketAndSetSendTime(kStartSeqNum));

    // With pacer there's two calls to history:
    // 1) When the NACK request arrived, use GetPacketState() to see if the
    //    packet is there and verify RTT constraints. Then we use the ssrc
    //    and sequence number to enqueue the retransmission in the pacer
    // 2) When the pacer determines that it is time to send the packet, it calls
    //    GetPacketAndSetSendTime().
    auto state = packet_history_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->packet_size, packet_size);
    EXPECT_EQ(state->capture_time_ms, capture_time_ms);

    clock_.AdvanceTimeMs(1);

    // First retransmission is always allowed.
    EXPECT_TRUE(packet_history_.GetPacketAndSetSendTime(kStartSeqNum));

    // Second retransmission: advance time to just before the time allowing retransmission.
    clock_.AdvanceTimeMs(kMinRetransmitIntervalMs - 1);
    EXPECT_FALSE(packet_history_.GetPacketState(kStartSeqNum));

    // Advance time to make the elapsed time since last retransmission >= RTT.
    clock_.AdvanceTimeMs(1);
    EXPECT_TRUE(packet_history_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_history_.GetPacketAndSetSendTime(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, MinResendTimeWithoutPacer) {
    static const int64_t kMinRetransmitIntervalMs = 100;

    packet_history_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    packet_history_.SetRttMs(kMinRetransmitIntervalMs);
    int capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    size_t packet_size = packet.size();
    clock_.AdvanceTimeMs(100);
    packet_history_.PutRtpPacket(std::move(packet), clock_.now_ms());

    clock_.AdvanceTimeMs(1);

    // First retransmission is always allowed.
    auto packet_out = packet_history_.GetPacketAndSetSendTime(kStartSeqNum);
    ASSERT_TRUE(packet_out);
    EXPECT_EQ(packet_size, packet_out->size());
    EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());

    // Second retransmission: advance time to just before the time allowing retransmission.
    clock_.AdvanceTimeMs(kMinRetransmitIntervalMs - 1);
    EXPECT_FALSE(packet_history_.GetPacketAndSetSendTime(kStartSeqNum));

    // Advance time to make the elapsed time since last retransmission >= RTT.
    clock_.AdvanceTimeMs(1);
    EXPECT_TRUE(packet_history_.GetPacketAndSetSendTime(kStartSeqNum));
}
    
} // namespace test
} // namespace naivertc
