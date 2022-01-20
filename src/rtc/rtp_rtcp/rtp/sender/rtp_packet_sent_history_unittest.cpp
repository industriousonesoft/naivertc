#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

#include <vector>

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
          packet_hist_(&clock_, /*enable_padding_prio=*/GetParam()) {}

    RtpPacketToSend CreateRtpPacket(uint16_t seq_num, int64_t capture_time_ms = 0) {
        RtpPacketToSend packet(nullptr);
        packet.set_sequence_number(seq_num);
        packet.set_capture_time_ms(capture_time_ms);
        packet.set_allow_retransmission(true);
        return packet;
    }

protected:
    SimulatedClock clock_;
    RtpPacketSentHistory packet_hist_;
};

MY_INSTANTIATE_TEST_SUITE_P(WithAndWithoutPaddingPrio, RtpPacketSentHistoryTest, ::testing::Bool());

MY_TEST_P(RtpPacketSentHistoryTest, SetStorageStatus) {
    EXPECT_EQ(StorageMode::DISABLE, packet_hist_.GetStorageMode());
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_EQ(StorageMode::STORE_AND_CULL, packet_hist_.GetStorageMode());
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_EQ(StorageMode::STORE_AND_CULL, packet_hist_.GetStorageMode());
    packet_hist_.SetStorePacketsStatus(StorageMode::DISABLE, 0);
    EXPECT_EQ(StorageMode::DISABLE, packet_hist_.GetStorageMode());
}

MY_TEST_P(RtpPacketSentHistoryTest, ClearHistoryAfterSetStorageStatus) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 0);
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // Changing storage status, even to the current one, will clear the history.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, StartSeqNumResetAfterReset) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    uint16_t seq_num = kStartSeqNum;
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num));
    EXPECT_TRUE(packet_hist_.GetPacketState(seq_num));

    // Changing store status, to clear the history.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_hist_.GetPacketState(seq_num));

    // Add a new packet
    seq_num = Unwrap(seq_num + 1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num));
    EXPECT_TRUE(packet_hist_.GetPacketState(seq_num));

    // Advance time past where packet expires.
    clock_.AdvanceTimeMs(RtpPacketSentHistory::kPacketCullingDelayFactor * RtpPacketSentHistory::kMinPacketDurationMs);

    seq_num = Unwrap(seq_num + 1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num));
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 2)));
}

MY_TEST_P(RtpPacketSentHistoryTest, NoStoreStatus) {
    EXPECT_EQ(StorageMode::DISABLE, packet_hist_.GetStorageMode());
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));
    // Packet should not be stored.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, GetRtpPacketNotStored) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_hist_.GetPacketState(0));
}

MY_TEST_P(RtpPacketSentHistoryTest, PutRtpPacket) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, GetRtpPacket) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 0);
    int64_t capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    auto packet_in = packet;

    packet_hist_.PutRtpPacket(std::move(packet));
    auto packet_out = packet_hist_.GetPacketAndSetSendTime(kStartSeqNum);
    EXPECT_TRUE(packet_out.has_value());
    EXPECT_EQ(packet_in, *packet_out);
    EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

MY_TEST_P(RtpPacketSentHistoryTest, PacketStateIsCorrect) {
    const uint32_t kSsrc = 9876543;
    const int64_t kRttMs = 100;
    
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    packet_hist_.SetRttMs(kRttMs);

    int64_t capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    packet.set_ssrc(kSsrc);
    packet.set_payload_type(98);
    const size_t packet_size = packet.size();

    clock_.AdvanceTimeMs(100);
    int64_t send_time_ms = clock_.now_ms();
    packet_hist_.PutRtpPacket(std::move(packet), send_time_ms);

    auto state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->rtp_sequence_number, kStartSeqNum);
    EXPECT_EQ(state->send_time_ms, send_time_ms);
    EXPECT_EQ(state->capture_time_ms, capture_time_ms);
    EXPECT_EQ(state->ssrc, kSsrc);
    EXPECT_EQ(state->packet_size, packet_size);
    EXPECT_EQ(state->num_retransmitted, 0);

    clock_.AdvanceTimeMs(1);
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));
    clock_.AdvanceTimeMs(kRttMs + 1);

    state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->num_retransmitted, 1);
}

MY_TEST_P(RtpPacketSentHistoryTest, MinResendTimeWithPacer) {
    static const int64_t kMinRetransmitIntervalMs = 100;

    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    packet_hist_.SetRttMs(kMinRetransmitIntervalMs);
    int capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    size_t packet_size = packet.size();
    packet_hist_.PutRtpPacket(std::move(packet));

    // First transmission call from pacer.
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));

    // With pacer there's two calls to history:
    // 1) When the NACK request arrived, use GetPacketState() to see if the
    //    packet is there and verify RTT constraints. Then we use the ssrc
    //    and sequence number to enqueue the retransmission in the pacer
    // 2) When the pacer determines that it is time to send the packet, it calls
    //    GetPacketAndSetSendTime().
    auto state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->packet_size, packet_size);
    EXPECT_EQ(state->capture_time_ms, capture_time_ms);

    clock_.AdvanceTimeMs(1);

    // First retransmission is always allowed.
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));

    // Second retransmission: advance time to just before the time allowing retransmission.
    clock_.AdvanceTimeMs(kMinRetransmitIntervalMs - 1);
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));

    // Advance time to make the elapsed time since last retransmission >= RTT.
    clock_.AdvanceTimeMs(1);
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, MinResendTimeWithoutPacer) {
    static const int64_t kMinRetransmitIntervalMs = 100;

    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);
    packet_hist_.SetRttMs(kMinRetransmitIntervalMs);
    int capture_time_ms = clock_.now_ms();
    auto packet = CreateRtpPacket(kStartSeqNum, capture_time_ms);
    size_t packet_size = packet.size();
    clock_.AdvanceTimeMs(100);
    packet_hist_.PutRtpPacket(std::move(packet), clock_.now_ms());

    clock_.AdvanceTimeMs(1);

    // First retransmission is always allowed.
    auto packet_out = packet_hist_.GetPacketAndSetSendTime(kStartSeqNum);
    ASSERT_TRUE(packet_out);
    EXPECT_EQ(packet_size, packet_out->size());
    EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());

    // Second retransmission: advance time to just before the time allowing retransmission.
    clock_.AdvanceTimeMs(kMinRetransmitIntervalMs - 1);
    EXPECT_FALSE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));

    // Advance time to make the elapsed time since last retransmission >= RTT.
    clock_.AdvanceTimeMs(1);
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, RemovesOldestSentPacketWhenAtMaxSize) {
    const size_t kMaxNumPackets = 10;
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kMaxNumPackets);
    
    // |packet_hist_| does not allow removing packets within kMinPacketDurationMs,
    // So in order to test capacity, make sure insetion spans this time.
    const int64_t kPacketIntervalMs = RtpPacketSentHistory::kMinPacketDurationMs / kMaxNumPackets;

    // Add packets until the buffer is full.
    for (size_t i = 0; i < kMaxNumPackets; ++i) {
        packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + i)), clock_.now_ms());
        clock_.AdvanceTimeMs(kPacketIntervalMs);
    }

    // The first packet still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // The oldest one should be removed if full
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + kMaxNumPackets)), clock_.now_ms());

    // The oldest packet should be gone, but the packet after than it still be there.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
}

MY_TEST_P(RtpPacketSentHistoryTest, RemovesOldestSentPacketWhenAtMaxCapacity) {
    const size_t kMaxNumPackets = RtpPacketSentHistory::kMaxCapacity;
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kMaxNumPackets);
    
    // Add packets until the buffer is full.
    for (size_t i = 0; i < kMaxNumPackets; ++i) {
        // Don't mark packets as sent, preventing them from being removed.
        packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + i)));
    }

    // The first packet still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // The oldest one should be removed if full
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + kMaxNumPackets)));

    // The oldest packet should be gone, but the packet after than it still be there.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
}

MY_TEST_P(RtpPacketSentHistoryTest, RemovesLowestPrioPaddingWhenAtMaxCapacity) {
    if (!GetParam()) {
        return;
    }
    
    const int64_t kRttMs = 1;
    // Test the absolute upper bound on number of packets in the prioritized set
    // of potential padding packets.
    const size_t kMaxPaddingPackets = RtpPacketSentHistory::kMaxPaddingtHistory;
    // Make sure there has enough space for all the potential padding packets.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kMaxPaddingPackets * 2);
    packet_hist_.SetRttMs(kRttMs);

    // Add packets until the max is reached, and then yet another one.
    for (size_t i = 0; i < kMaxPaddingPackets + 1; ++i) {
        // Mark packets as sent.
        packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + i)), clock_.now_ms());
    }

    // Advanve time to allow retransmission/padding.
    clock_.AdvanceTimeMs(kRttMs);

    // The oldest one (i = 0) will be least prioritized and has fallen out of 
    // the priority set.
    for (size_t i = kMaxPaddingPackets - 1; i > 0; --i) {
        auto packet = packet_hist_.GetPayloadPaddingPacket();
        ASSERT_TRUE(packet);
        EXPECT_EQ(packet->sequence_number(), Unwrap(kStartSeqNum + i + 1));
    }

    auto packet = packet_hist_.GetPayloadPaddingPacket();
    ASSERT_TRUE(packet);
    EXPECT_EQ(packet->sequence_number(), Unwrap(kStartSeqNum + kMaxPaddingPackets));
}

MY_TEST_P(RtpPacketSentHistoryTest, DontRemoveUnsentPacket) {
    const size_t kMaxNumPackets = 10;
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kMaxNumPackets);

    // Add packets until the buffer is full.
    for (size_t i = 0; i < kMaxNumPackets; ++i) {
        // Mark packets as unsent.
        packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + i)));
    }
    clock_.AdvanceTimeMs(RtpPacketSentHistory::kMinPacketDurationMs);

    // First packet should still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // History is full, but old packets not sent, so allow expansion.
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + kMaxNumPackets)), clock_.now_ms());
    // The oldest one is not removed yet.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // Set all packets as sent.
    for (size_t i = 0; i <= kMaxNumPackets; ++i) {
        EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(Unwrap(kStartSeqNum + i)));
    }
    // Advance time past min packet duration time.
    clock_.AdvanceTimeMs(RtpPacketSentHistory::kMinPacketDurationMs);

    // Add a new packet which means the two oldest packets will be culled.
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + kMaxNumPackets + 1)), clock_.now_ms());

    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_FALSE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 2)));

}

MY_TEST_P(RtpPacketSentHistoryTest, DontRemoveTooRecentlyTransmittedPackets) {
    // The RTT is set to zero as default, so the packet duration will be |kMinPacketDurationMs|.
    const int64_t kPacketDurationMs = RtpPacketSentHistory::kMinPacketDurationMs;
    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());
    // Advance time to just before removal time.
    clock_.AdvanceTimeMs(kPacketDurationMs - 1);

    // Add a new packet to trigger culling.
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 1)), clock_.now_ms());
    // The first packet should still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // Advance time to where packet will be eligible for removal and try again.
    clock_.AdvanceTimeMs(1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 2)), clock_.now_ms());

    // The first packet should be gone, but the next one still there.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
}

MY_TEST_P(RtpPacketSentHistoryTest, DontRemoveTooRecentlyTransmittedPacketsWithHighRtt) {
    const int64_t kRttMs = RtpPacketSentHistory::kMinPacketDurationMs * 2;
    // As the RTT is too high enough, the packet duration will be calculated with RTT.
    const int64_t kPacketDurationMs = kRttMs * RtpPacketSentHistory::kMinPacketDurationRttFactor;

    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);
    packet_hist_.SetRttMs(kRttMs);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());
    // Advance time to just before removal time.
    clock_.AdvanceTimeMs(kPacketDurationMs - 1);

    // Add a new packet to trigger culling.
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 1)), clock_.now_ms());
    // The first packet should still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // Advance time to where packet will be eligible for removal and try again.
    clock_.AdvanceTimeMs(1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 2)), clock_.now_ms());

    // The first packet should be gone, but the next one still there.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
}

MY_TEST_P(RtpPacketSentHistoryTest, RemoveOldWithCulling) {
    const size_t kMaxNumPackets = 10;
    // Enable culling. Even without feedback, this can trigger early removal.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kMaxNumPackets);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());

    int64_t kMaxPacketDurationMs = RtpPacketSentHistory::kMinPacketDurationMs * RtpPacketSentHistory::kPacketCullingDelayFactor;
     // Advance time to just before removal time.
    clock_.AdvanceTimeMs(kMaxPacketDurationMs - 1);

    // The first packet should still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // Advance to where packet can be culled, even if buffer is not full.
    clock_.AdvanceTimeMs(1);

    // Add a new packet to trigger culling.
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 1)), clock_.now_ms());

    // The first packet should be gone.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, RemoveOldWithCullingWithHighRtt) {
    const size_t kMaxNumPackets = 10;
    const int64_t kRttMs = RtpPacketSentHistory::kMinPacketDurationMs * 2;
    // Enable culling. Even without feedback, this can trigger early removal.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kMaxNumPackets);
    packet_hist_.SetRttMs(kRttMs);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());

    int64_t kPacketLifeTimeMs = kRttMs * 
                                RtpPacketSentHistory::kMinPacketDurationRttFactor * 
                                RtpPacketSentHistory::kPacketCullingDelayFactor;
     // Advance time to just before removal time.
    clock_.AdvanceTimeMs(kPacketLifeTimeMs - 1);

    // The first packet should still be there.
    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));

    // Advance to where packet can be culled, even if buffer is not full.
    clock_.AdvanceTimeMs(1);

    // Add a new packet to trigger culling.
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 1)), clock_.now_ms());

    // The first packet should be gone.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, CullWithAcks) {
    const int64_t kPacketLifeTimeMs = RtpPacketSentHistory::kMinPacketDurationMs * RtpPacketSentHistory::kPacketCullingDelayFactor;

    const int64_t start_time = clock_.now_ms();
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);

    // Insert three packets every 33ms, and mark them as sent immediatly.
    uint16_t seq_num = kStartSeqNum;
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num));
    packet_hist_.GetPacketAndSetSendTime(seq_num);
    clock_.AdvanceTimeMs(33);
    seq_num = Unwrap(kStartSeqNum + 1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num));
    packet_hist_.GetPacketAndSetSendTime(seq_num);
    clock_.AdvanceTimeMs(33);
    seq_num = Unwrap(kStartSeqNum + 2);
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num));
    packet_hist_.GetPacketAndSetSendTime(seq_num);
    clock_.AdvanceTimeMs(33);

    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 2)));

    // Remove the middle one using ACK, and check only that packet is gone.
    std::vector<uint16_t> acked_seq_nums = {Unwrap(kStartSeqNum + 1)};
    packet_hist_.CullAckedPackets(acked_seq_nums);

    EXPECT_TRUE(packet_hist_.GetPacketState(kStartSeqNum));
    EXPECT_FALSE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 1)));
    EXPECT_TRUE(packet_hist_.GetPacketState(Unwrap(kStartSeqNum + 2)));
}

MY_TEST_P(RtpPacketSentHistoryTest, SetPendingTransmisstionState) {
    const int64_t kRttMs = RtpPacketSentHistory::kMinPacketDurationMs * 2;
    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);
    packet_hist_.SetRttMs(kRttMs);
    
    // Add a packet and mark as unsend, indicating it's in the pacer queue.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum));

    auto state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_TRUE(state->pending_transmission);

    // Packet sent, the state should be back to non-pending.
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));
    state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_FALSE(state->pending_transmission);

    // Advance time for a retransmission.
    clock_.AdvanceTimeMs(kRttMs);
    EXPECT_TRUE(packet_hist_.SetPendingTransmission(kStartSeqNum));
    state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_TRUE(state->pending_transmission);

    // Packet sent.
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));
    // Too early for retransmission.
    EXPECT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));

    // Retransmission allowed again, it's not in a pending state.
    clock_.AdvanceTimeMs(kRttMs);
    state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_FALSE(state->pending_transmission);
}

MY_TEST_P(RtpPacketSentHistoryTest, GetPacketAndSetSent) {
    const int64_t kRttMs = RtpPacketSentHistory::kMinPacketDurationMs * 2;
    packet_hist_.SetRttMs(kRttMs);

    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());

    // Retransmission request, first retransmission is allowed immdidately.
    EXPECT_TRUE(packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum));

    // Packet not set yet, new retransmission not allowed.
    clock_.AdvanceTimeMs(kRttMs);
    EXPECT_FALSE(packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum));

    // Mark as sent, but too early for retransmission.
    packet_hist_.MarkPacketAsSent(kStartSeqNum);
    EXPECT_FALSE(packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum));

    // Advance time for retransmission.
    clock_.AdvanceTimeMs(kRttMs);
    EXPECT_TRUE(packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, GetPacketWithEncapsulation) {
    const uint32_t kSsrc = 123456;
    const uint32_t kRetransmitSsrc = 234567;
    const int64_t kRttMs = RtpPacketSentHistory::kMinPacketDurationMs * 2;
    packet_hist_.SetRttMs(kRttMs);

    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);
    
    // Add a new and mark as sent.
    auto packet = CreateRtpPacket(kStartSeqNum);
    packet.set_ssrc(kSsrc);
    packet_hist_.PutRtpPacket(std::move(packet), clock_.now_ms());

    // Retransmission request, simulate an RTX-like encapsulation,
    // were the packet is sent on a different SSRC.
    auto retransmit_packet = packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum, [kRetransmitSsrc](const RtpPacketToSend& packet){
        auto encapsulated_packet = packet;
        encapsulated_packet.set_ssrc(kRetransmitSsrc);
        return encapsulated_packet;
    });
    ASSERT_TRUE(retransmit_packet);
    EXPECT_EQ(retransmit_packet->ssrc(), kRetransmitSsrc);
}

MY_TEST_P(RtpPacketSentHistoryTest, GetPacketWithEncapsulationAbortOnNullptr) {
    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());

    // Retransmission request, but the encapsulator determins that this packet
    // is not suitable for retransmisson (bandwith exhausted?) so the retransmit
    // is aborted and the packet is not marked as pending.
    EXPECT_FALSE(packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum, [](const RtpPacketToSend&){ return std::nullopt; }));

    // New try, this time getting the packet should work, and it should not be
    // blocked due to any pending status.
    EXPECT_TRUE(packet_hist_.GetPacketAndMarkAsPending(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, DontRemovePendingTransmissions) {
    const int64_t kRttMs = RtpPacketSentHistory::kMinPacketDurationMs * 2;
    const int64_t kPacketTimeoutMs = kRttMs * RtpPacketSentHistory::kMinPacketDurationRttFactor;

    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);
    packet_hist_.SetRttMs(kRttMs);

    // Add a packet and mark as sent.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());
    
    // Advance time to just before packet timeout.
    clock_.AdvanceTimeMs(kPacketTimeoutMs - 1);
    // Mark as enquenced in pacer.
    EXPECT_TRUE(packet_hist_.SetPendingTransmission(kStartSeqNum));

    // Advance time to where packet would have timed out.
    // it should still be there and pending.
    clock_.AdvanceTimeMs(1);
    auto state = packet_hist_.GetPacketState(kStartSeqNum);
    ASSERT_TRUE(state);
    EXPECT_TRUE(state->pending_transmission);

    // Packet sent. Now it can be removed.
    EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(kStartSeqNum));
    // Too early for next retransmission.
    ASSERT_FALSE(packet_hist_.GetPacketState(kStartSeqNum));
}

MY_TEST_P(RtpPacketSentHistoryTest, PrioritizedPayloadPadding) {
    if (!GetParam()) {
        return;
    }

    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);

    // Add two sent packets, 1 ms apart.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());
    clock_.AdvanceTimeMs(1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(Unwrap(kStartSeqNum + 1)), clock_.now_ms());
    clock_.AdvanceTimeMs(1);

    // Latest packet given equal retransmission count.
    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), Unwrap(kStartSeqNum + 1));

    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), kStartSeqNum);

    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), Unwrap(kStartSeqNum + 1));

    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), kStartSeqNum);

    // Remove newest packet.
    packet_hist_.CullAckedPackets(std::vector<uint16_t>{kStartSeqNum + 1});

    // Only older packet left.
    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), kStartSeqNum);

    packet_hist_.CullAckedPackets(std::vector<uint16_t>{kStartSeqNum});

    EXPECT_FALSE(packet_hist_.GetPayloadPaddingPacket());
}

MY_TEST_P(RtpPacketSentHistoryTest, NoPendingPacketAsPadding) {
    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);

    // Add two sent packets, 1 ms apart.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());
    clock_.AdvanceTimeMs(1);

    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), kStartSeqNum);

    // If packet is pending retransmission, don't try to use it as padding.
    packet_hist_.SetPendingTransmission(kStartSeqNum);
    EXPECT_FALSE(packet_hist_.GetPayloadPaddingPacket());

    // Packet sent and mark it as no longer pending, should be used as paddding again.
    packet_hist_.GetPacketAndSetSendTime(kStartSeqNum);
    EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), kStartSeqNum);
}

MY_TEST_P(RtpPacketSentHistoryTest, PayloadPaddingWithEncapsulation) {
    // Set size to remove old packets as soon as possible.
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 1);

    // Add two sent packets, 1 ms apart.
    packet_hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), clock_.now_ms());
    clock_.AdvanceTimeMs(1);

    // Aborted padding.
    EXPECT_FALSE(packet_hist_.GetPayloadPaddingPacket([](const RtpPacketToSend&){ return std::nullopt; }));

    uint16_t padding_seq_num = Unwrap(kStartSeqNum + 1);
    auto padding_packet = packet_hist_.GetPayloadPaddingPacket([padding_seq_num](const RtpPacketToSend& packet){
        auto encapsulated_packet = packet;
        encapsulated_packet.set_sequence_number(padding_seq_num);
        return encapsulated_packet;
    });
    ASSERT_TRUE(padding_packet);
    EXPECT_EQ(padding_packet->sequence_number(), padding_seq_num);
}

MY_TEST_P(RtpPacketSentHistoryTest, NackAfterAckIsNoop) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 2);

    // Add two sent packets.
    uint16_t seq_num = kStartSeqNum;
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num), clock_.now_ms());
    seq_num = Unwrap(kStartSeqNum + 1);
    packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num), clock_.now_ms());

    // Remove newest one.
    packet_hist_.CullAckedPackets(std::vector<uint16_t>{seq_num});
    // Retransmission request for already acked packet should be noop (No Operation).
    EXPECT_FALSE(packet_hist_.GetPacketAndMarkAsPending(seq_num));
}

MY_TEST_P(RtpPacketSentHistoryTest, OutOfOrderInsertAndRemoval) {
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, 10);

    // Insert packets, out of order, including both forwards and backwards
    // sequence number wraps.
    const int seq_offsets[] = {0, 1, -1, 2, -2, 3, -3};
    const int64_t start_time_ms = clock_.now_ms();

    // Insert pakcets out of order.
    for (int offset : seq_offsets) {
        uint16_t seq_num = Unwrap(kStartSeqNum + offset);
        packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num), clock_.now_ms());
        EXPECT_TRUE(packet_hist_.GetPacketAndSetSendTime(seq_num));
        clock_.AdvanceTimeMs(33);
    }

    // Remove packets out of order.
    int64_t expected_time_offset_ms = 0;
    for (int offset : seq_offsets) {
        uint16_t seq_num = Unwrap(kStartSeqNum + offset);
        auto state = packet_hist_.GetPacketState(seq_num);
        ASSERT_TRUE(state);
        EXPECT_EQ(state->send_time_ms, start_time_ms + expected_time_offset_ms);
        packet_hist_.CullAckedPackets(std::vector<uint16_t>{seq_num});
        expected_time_offset_ms += 33;
    }
    
    // Check all packets was gone.
    for (int offset : seq_offsets) {
        uint16_t seq_num = Unwrap(kStartSeqNum + offset);
        ASSERT_FALSE(packet_hist_.GetPacketState(seq_num));
    }
}

MY_TEST_P(RtpPacketSentHistoryTest, LastPacketAsPaddingWithPrioOff) {
    if (GetParam()) {
        return;
    }

    const size_t kNumPackets = 10;
    packet_hist_.SetStorePacketsStatus(StorageMode::STORE_AND_CULL, kNumPackets);

    // No packet should be returned before adding new packet.
    EXPECT_FALSE(packet_hist_.GetPayloadPaddingPacket());

    for (size_t i = 0; i < kNumPackets; i++) {
        uint16_t seq_num = Unwrap(kStartSeqNum + i);
        packet_hist_.PutRtpPacket(CreateRtpPacket(seq_num), clock_.now_ms());
        packet_hist_.MarkPacketAsSent(seq_num);
        clock_.AdvanceTimeMs(1);

        // Last packet always returned.
        EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), seq_num);
        EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), seq_num);
        EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), seq_num);
    }

    // Remove packets from the end, the last in the list should be returned.
    uint16_t expected_seq_num = 0;
    for (size_t i = kNumPackets - 1; i > 0; --i) {
        packet_hist_.CullAckedPackets(std::vector<uint16_t>{Unwrap(kStartSeqNum + i)});

        expected_seq_num = Unwrap(kStartSeqNum + i - 1);
        EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), expected_seq_num);
        EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), expected_seq_num);
        EXPECT_EQ(packet_hist_.GetPayloadPaddingPacket()->sequence_number(), expected_seq_num);
    }

    // Remove the last packet in the packet, and no packet should be returned.
    packet_hist_.CullAckedPackets(std::vector<uint16_t>{kStartSeqNum});
    EXPECT_FALSE(packet_hist_.GetPayloadPaddingPacket());
}
    
} // namespace test
} // namespace naivertc
