#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

using namespace ::testing;

namespace naivertc {
namespace test {
namespace {
    
constexpr int kDefaultPayloadType = 100;
constexpr int kFlexfectPayloadType = 110;
constexpr uint16_t kStartSequenceNumber = 33;
constexpr uint32_t kSsrc = 725242;
constexpr uint32_t kRtxSsrc = 12345;
constexpr uint32_t kFlexFecSsrc = 23456;

enum : int {
    kTransportSequenceNumberExtensionId = 1,
    kAbsoluteSendTimeExtensionId,
    kTransmissionOffsetExtensionId,
    kVideoTimingExtensionId,
};

// MockStreamDataCountersObserver
class MockStreamDataCountersObserver : public RtpStreamDataCountersObserver {
public:
    MOCK_METHOD(void, 
                OnStreamDataCountersUpdated, 
                (const RtpStreamDataCounters& counter, uint32_t ssrc), 
                (override));
};

// MockSendDelayObserver
class MockSendDelayObserver : public RtpSendDelayObserver {
public:
    MOCK_METHOD(void, 
                OnSendDelayUpdated, 
                (int64_t avg_delay_ms, int64_t max_delay_ms, int64_t total_delay_ms, uint32_t ssrc), 
                (override));
};

// MockSendPacketObserver
class MockSendPacketObserver : public RtpSendPacketObserver {
public:
    MOCK_METHOD(void,
                OnSendPacket,
                (uint16_t packet_id, int64_t capture_time_ms, uint32_t ssrc),
                (override));
};

// MockSendBitratesObserver
class MockSendBitratesObserver : public RtpSendBitratesObserver {
public:
    MOCK_METHOD(void, 
                OnSendBitratesUpdated, 
                (uint32_t total_bitrate_bps, uint32_t retransmit_bitrate_bps, uint32_t ssrc), 
                (override));
};

// MockPacketSendStatsObserver 
class MockTransportFeedbackObserver : public RtpTransportFeedbackObserver {
public:
    MOCK_METHOD(void, 
                OnAddPacket, 
                (const RtpTransportFeedback&), 
                (override));
};

// SendTransportImpl
class SendTransportImpl : public MediaTransport {
public:
    SendTransportImpl(rtp::HeaderExtensionMap* header_extension_map) 
        : total_bytes_sent_(0),
          header_extension_map_(header_extension_map) {}

    bool SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        total_bytes_sent_ += packet.size();
        RtpPacketReceived recv_packet(header_extension_map_);
        EXPECT_TRUE(recv_packet.Parse(std::move(packet)));
        last_recv_packet_.emplace(std::move(recv_packet));
        return true;
    }

    bool SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        RTC_NOTREACHED();
    }

    std::optional<RtpPacketReceived> last_packet() const {
        return last_recv_packet_;
    };

private:
    int64_t total_bytes_sent_;
    std::optional<RtpPacketReceived> last_recv_packet_;
    rtp::HeaderExtensionMap* const header_extension_map_;
};

} // namespace

class T(RtpPacketEgresserTest) : public ::testing::TestWithParam<bool> {
protected:
    T(RtpPacketEgresserTest)() 
        : clock_(123456),
          send_transport_(&header_extension_map_),
          packet_history_(&clock_, /*enable_rtx_padding_prioritization=*/true),
          seq_num_(kStartSequenceNumber) {};

    std::unique_ptr<RtpPacketEgresser> CreateRtpPacketEgresser() {
        return std::make_unique<RtpPacketEgresser>(DefaultConfig(), &packet_history_);
    } 

    RtpConfiguration DefaultConfig() {
        RtpConfiguration config;
        config.audio = false;
        config.clock = &clock_;
        config.send_side_bwe_with_overhead = GetParam();
        config.local_media_ssrc = kSsrc;
        config.rtx_send_ssrc = kRtxSsrc;
        config.fec_generator = nullptr;
        config.send_transport = &send_transport_;
        config.send_bitrates_observer = nullptr; // Disable the repeating task.
        config.send_delay_observer = &send_delay_observer_;
        config.send_packet_observer = &send_packet_observer_;
        config.send_bitrates_observer = &send_bitrates_observer_;
        config.transport_feedback_observer = &transport_feedback_observer_;
        config.stream_data_counters_observer = &stream_data_counters_observer_;
        return config;
    }

    RtpPacketToSend BuildRtpPacket(bool marker_bit,
                                   int64_t capture_time_ms) {
        RtpPacketToSend packet(&header_extension_map_);
        packet.set_ssrc(kSsrc);
        packet.ReserveExtension<rtp::AbsoluteSendTime>();
        packet.ReserveExtension<rtp::TransmissionTimeOffset>();

        packet.set_payload_type(kDefaultPayloadType);
        packet.set_packet_type(RtpPacketType::VIDEO);
        packet.set_marker(marker_bit);
        packet.set_timestamp(capture_time_ms * 90);
        packet.set_capture_time_ms(capture_time_ms);
        packet.set_sequence_number(seq_num_++);
        return packet;
    }

    RtpPacketToSend BuildRtpPacket() {
        return BuildRtpPacket(true, clock_.now_ms());
    }

protected:
    SimulatedClock clock_;
    NiceMock<MockStreamDataCountersObserver> stream_data_counters_observer_;
    NiceMock<MockSendDelayObserver> send_delay_observer_;
    NiceMock<MockSendPacketObserver> send_packet_observer_;
    NiceMock<MockSendBitratesObserver> send_bitrates_observer_;
    NiceMock<MockTransportFeedbackObserver> transport_feedback_observer_;
    SendTransportImpl send_transport_;
    RtpPacketHistory packet_history_;
    rtp::HeaderExtensionMap header_extension_map_;
    uint16_t seq_num_;
};

MY_INSTANTIATE_TEST_SUITE_P(WithOrWithoutOverhead, RtpPacketEgresserTest, ::testing::Bool());

MY_TEST_P(RtpPacketEgresserTest, PacketSendStatsObserverGetsCorrectByteCount) {
    const size_t kRtpOverheadBytesPerPacket = 12 + 8;
    const size_t kPayloadSize = 1400;
    const uint16_t kTransportSeqNum = 17;

    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId, 
                                        rtp::TransportSequenceNumber::kUri);

    const size_t expected_bytes = GetParam() ? kPayloadSize + kRtpOverheadBytesPerPacket
                                             : kPayloadSize;

    EXPECT_CALL(transport_feedback_observer_, 
                OnAddPacket(AllOf(
                    Field(&RtpTransportFeedback::ssrc, kSsrc),
                    Field(&RtpTransportFeedback::packet_id, kTransportSeqNum),
                    Field(&RtpTransportFeedback::seq_num, kStartSequenceNumber),
                    Field(&RtpTransportFeedback::packet_size, expected_bytes)
                )));

    auto packet = BuildRtpPacket();
    packet.SetExtension<rtp::TransportSequenceNumber>(kTransportSeqNum);
    packet.AllocatePayload(kPayloadSize);

    auto sender = CreateRtpPacketEgresser();
    sender->SendPacket(std::move(packet));
}

MY_TEST_P(RtpPacketEgresserTest, OnSendDelayUpdated) {
    auto sender = CreateRtpPacketEgresser();

    // Send packet with 10 ms send-side delay. The average, max and total should
    // be 10 ms.
    EXPECT_CALL(send_delay_observer_, OnSendDelayUpdated(10, 10, 10, kSsrc));
    int64_t capture_time_ms = clock_.now_ms();
    clock_.AdvanceTimeMs(10);
    sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms));

    // Send another packet with 20 ms delay. The average, max and total should be
    // 15, 20 and 30 ms respectively.
    EXPECT_CALL(send_delay_observer_, OnSendDelayUpdated(15, 20, 30, kSsrc));
    capture_time_ms = clock_.now_ms();
    clock_.AdvanceTimeMs(20);
    sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms));

    // Send another packet at almost the same time, which replaces the last packet.
    // Since this packet has 0 ms delay, the average is now 5 ms and the max is 10 ms,
    // the total stays the same though.
    EXPECT_CALL(send_delay_observer_, OnSendDelayUpdated(5, 10, 30, kSsrc));
    capture_time_ms = clock_.now_ms();
    sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms));

    // Send a packet 1 second later, the earlier packets should have timed out,
    // so both max and average should be the delay of this packet.
    // The total keeps increasing.
    clock_.AdvanceTimeMs(1000);
    EXPECT_CALL(send_delay_observer_, OnSendDelayUpdated(2, 2, 32, kSsrc));
    capture_time_ms = clock_.now_ms();
    clock_.AdvanceTimeMs(2);
    sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms));
}

MY_TEST_P(RtpPacketEgresserTest, OnSendPacketUpdated) {
    auto sender = CreateRtpPacketEgresser();

    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId, rtp::TransportSequenceNumber::kUri);

    const uint16_t kTransportSeqNum = 123;
    const int64_t capture_time_ms = clock_.now_ms();
    EXPECT_CALL(send_packet_observer_,
                OnSendPacket(kTransportSeqNum, capture_time_ms, kSsrc));
    auto packet = BuildRtpPacket(/*marker=*/true, capture_time_ms);
    packet.SetExtension<rtp::TransportSequenceNumber>(kTransportSeqNum);
    sender->SendPacket(std::move(packet));
}

MY_TEST_P(RtpPacketEgresserTest, OnSendPacketNotUpdatedForRetransmission) {
    auto sender = CreateRtpPacketEgresser();

    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId, rtp::TransportSequenceNumber::kUri);

    const uint16_t kTransportSeqNum = 123;
    EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(0);

    auto packet = BuildRtpPacket();
    packet.SetExtension<rtp::TransportSequenceNumber>(kTransportSeqNum);
    packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    packet.set_retransmitted_sequence_number(packet.sequence_number());

    sender->SendPacket(std::move(packet));
}

MY_TEST_P(RtpPacketEgresserTest, ReportsFecRate) {
    constexpr int kNumPackets = 10;
    constexpr TimeDelta kTimeBetweenPackets = TimeDelta::Millis(33);

    auto sender = CreateRtpPacketEgresser();
    size_t total_fec_bytes_sent = 0;

    for (size_t i = 0; i < kNumPackets; ++i) {
        auto media_packet = BuildRtpPacket();
        media_packet.set_packet_type(RtpPacketType::VIDEO);
        media_packet.SetPayloadSize(500);
        sender->SendPacket(std::move(media_packet));

        auto fec_packet = BuildRtpPacket();
        fec_packet.set_packet_type(RtpPacketType::FEC);
        fec_packet.SetPayloadSize(123);
        sender->SendPacket(std::move(fec_packet));
        total_fec_bytes_sent += fec_packet.size();

        clock_.AdvanceTime(kTimeBetweenPackets);
    }

    double expected_send_bitrate_bps = (total_fec_bytes_sent * 8000.0) / (kTimeBetweenPackets.ms() * kNumPackets);
    EXPECT_NEAR(sender->GetSendBitrate(RtpPacketType::FEC).bps<double>(), expected_send_bitrate_bps, 100);

}

MY_TEST_P(RtpPacketEgresserTest, SendBitratesObserver) {
    auto sender = CreateRtpPacketEgresser();

    // Simulate kNumPackets sent with kPacketInterval intervals, with the
    // number of packets selected so that we fill (but don't overflow) the one
    // second averaging window.
    const TimeDelta kWindowSize = TimeDelta::Seconds(1);
    const TimeDelta kPacketInterval = TimeDelta::Millis(20);
    const int kNumPackets = (kWindowSize - kPacketInterval) / kPacketInterval;

    size_t total_bytes_sent = 0;

    for (int i = 0; i < kNumPackets; i++) {
        auto packet = BuildRtpPacket();
        packet.SetPayloadSize(500);
        // Mark all the packets as retransmissions that will cause total and 
        // retransmitted rate to be equal.
        packet.set_packet_type(RtpPacketType::RETRANSMISSION);
        packet.set_retransmitted_sequence_number(packet.sequence_number());
        total_bytes_sent += packet.size();

        EXPECT_CALL(send_bitrates_observer_, OnSendBitratesUpdated(_, _, kSsrc))
            .WillOnce([&](uint32_t total_bitrate_bps, uint32_t retransmit_bitrate_bps, uint32_t /*ssrc*/){
                TimeDelta window_size = kPacketInterval * i + TimeDelta::Millis(1);
                // Only one single samples is not enough for valid estimate.
                const double expected_bitrate_bps = i == 0 ? 0.0 : total_bytes_sent * 8000.0 / window_size.ms();

                EXPECT_NEAR(total_bitrate_bps, expected_bitrate_bps, 500);
                EXPECT_NEAR(retransmit_bitrate_bps, expected_bitrate_bps, 500);
            });

        sender->SendPacket(std::move(packet));
        clock_.AdvanceTime(kPacketInterval);
    }

}

MY_TEST_P(RtpPacketEgresserTest, DoesNotPutNotRetransmittablePacketsInHistory) {
    auto sender = CreateRtpPacketEgresser();
    packet_history_.SetStorePacketsStatus(RtpPacketHistory::StorageMode::STORE_AND_CULL, 10);

    auto packet = BuildRtpPacket();
    uint16_t seq_num = packet.sequence_number();
    // Not retransmittable
    packet.set_allow_retransmission(false);
    sender->SendPacket(std::move(packet));

    EXPECT_FALSE(packet_history_.GetPacketState(seq_num));
}

MY_TEST_P(RtpPacketEgresserTest, PutsRetransmittablePacketsInHistory) {
    auto sender = CreateRtpPacketEgresser();
    packet_history_.SetStorePacketsStatus(RtpPacketHistory::StorageMode::STORE_AND_CULL, 10);

    auto packet = BuildRtpPacket();
    uint16_t seq_num = packet.sequence_number();
    // Retransmittable
    packet.set_allow_retransmission(true);
    sender->SendPacket(std::move(packet));

    EXPECT_THAT(packet_history_.GetPacketState(seq_num), 
                Optional(Field(&RtpPacketHistory::PacketState::pending_transmission, false)));
}

MY_TEST_P(RtpPacketEgresserTest, UpdateExtenstionWhenSendingPacket) {
    auto sender = CreateRtpPacketEgresser();

    header_extension_map_.RegisterByUri(kAbsoluteSendTimeExtensionId, rtp::AbsoluteSendTime::kUri);
    header_extension_map_.RegisterByUri(kTransmissionOffsetExtensionId, rtp::TransmissionTimeOffset::kUri);
    // TODO: Add VideoTimingExtension

    const int64_t capture_time_ms = clock_.now_ms();
    auto packet = BuildRtpPacket(true, capture_time_ms);
    ASSERT_TRUE(packet.HasExtension<rtp::AbsoluteSendTime>());
    ASSERT_TRUE(packet.HasExtension<rtp::TransmissionTimeOffset>());

    const int64_t kDiffMs = 10;
    clock_.AdvanceTimeMs(kDiffMs);

    sender->SendPacket(std::move(packet));

    auto recv_packet = send_transport_.last_packet();
    ASSERT_TRUE(recv_packet);
    ASSERT_TRUE(recv_packet->HasExtension<rtp::AbsoluteSendTime>());
    ASSERT_TRUE(recv_packet->HasExtension<rtp::TransmissionTimeOffset>());
    EXPECT_EQ(recv_packet->GetExtension<rtp::AbsoluteSendTime>(), rtp::AbsoluteSendTime::MsTo24Bits(clock_.now_ms()));
    EXPECT_EQ(recv_packet->GetExtension<rtp::TransmissionTimeOffset>(), kDiffMs * 90);
}

MY_TEST_P(RtpPacketEgresserTest, DoseNotPutNonMediaPacketInHistory) {
    auto sender = CreateRtpPacketEgresser();
    packet_history_.SetStorePacketsStatus(RtpPacketHistory::StorageMode::STORE_AND_CULL, 10);

    // Non-media packet, even marked as retransmittable, are not put
    // into the packet history.
    auto retransmitted_packet = BuildRtpPacket();
    uint16_t seq_num = retransmitted_packet.sequence_number();
    retransmitted_packet.set_allow_retransmission(true);
    retransmitted_packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    retransmitted_packet.set_retransmitted_sequence_number(retransmitted_packet.sequence_number());
    sender->SendPacket(std::move(retransmitted_packet));

    EXPECT_FALSE(packet_history_.GetPacketState(seq_num));

    // FEC packet
    auto fec_packet = BuildRtpPacket();
    seq_num = fec_packet.sequence_number();
    fec_packet.set_allow_retransmission(true);
    fec_packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    fec_packet.set_retransmitted_sequence_number(fec_packet.sequence_number());
    sender->SendPacket(std::move(fec_packet));

    EXPECT_FALSE(packet_history_.GetPacketState(seq_num));

    // Padding packet
    auto padding_packet = BuildRtpPacket();
    seq_num = padding_packet.sequence_number();
    padding_packet.set_allow_retransmission(true);
    padding_packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    padding_packet.set_retransmitted_sequence_number(padding_packet.sequence_number());
    sender->SendPacket(std::move(padding_packet));

    EXPECT_FALSE(packet_history_.GetPacketState(seq_num));
}

MY_TEST_P(RtpPacketEgresserTest, UpdateSendStatusOfRetransmittedPacktes) {
    auto sender = CreateRtpPacketEgresser();
    packet_history_.SetStorePacketsStatus(RtpPacketHistory::StorageMode::STORE_AND_CULL, 10);

    auto media_packet = BuildRtpPacket();
    uint16_t seq_num = media_packet.sequence_number();
    media_packet.set_allow_retransmission(true);
    sender->SendPacket(std::move(media_packet));

    EXPECT_THAT(packet_history_.GetPacketState(seq_num), 
                Optional(Field(&RtpPacketHistory::PacketState::pending_transmission, false)));

    // Simulate a retransmission, marking the packet as pending.
    auto retransmitted_packet = packet_history_.GetPacketAndMarkAsPending(seq_num);
    ASSERT_TRUE(retransmitted_packet);
    retransmitted_packet->set_retransmitted_sequence_number(seq_num);
    retransmitted_packet->set_packet_type(RtpPacketType::RETRANSMISSION);

    EXPECT_THAT(packet_history_.GetPacketState(seq_num), 
                Optional(Field(&RtpPacketHistory::PacketState::pending_transmission, true)));

    // Simulate packet leaving pacer, the packet should be marked as non-pending.
    sender->SendPacket(std::move(*retransmitted_packet));
    EXPECT_THAT(packet_history_.GetPacketState(seq_num), 
                Optional(Field(&RtpPacketHistory::PacketState::pending_transmission, false)));
}

MY_TEST_P(RtpPacketEgresserTest, StreamDataCountersCallbacks) {
    auto sender = CreateRtpPacketEgresser();

    const RtpPacketCounter kEmptyCounter;
    RtpPacketCounter expected_transmitted_counter;
    RtpPacketCounter expected_retransmission_counter;

    // Send a media packet.
    auto media_packet = BuildRtpPacket();
    media_packet.SetPayloadSize(666);
    expected_transmitted_counter.num_packets += 1;
    expected_transmitted_counter.payload_bytes += media_packet.payload_size();
    expected_transmitted_counter.header_bytes += media_packet.header_size();

    EXPECT_CALL(
        stream_data_counters_observer_,
        OnStreamDataCountersUpdated(AllOf(Field(&RtpStreamDataCounters::transmitted,
                                                expected_transmitted_counter),
                                          Field(&RtpStreamDataCounters::retransmitted,
                                                expected_retransmission_counter),
                                          Field(&RtpStreamDataCounters::fec, kEmptyCounter)),
                                    kSsrc));
    sender->SendPacket(std::move(media_packet));
    clock_.AdvanceTimeMs(10);

    // Send a retransmission. Retransmissions are counted into both transmitted
    // and retransmitted packet statistics.
    auto retransmission_packet = BuildRtpPacket();
    retransmission_packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    retransmission_packet.set_retransmitted_sequence_number(retransmission_packet.sequence_number());
    retransmission_packet.SetPayloadSize(237);
    expected_transmitted_counter.num_packets += 1;
    expected_transmitted_counter.payload_bytes += retransmission_packet.payload_size();
    expected_transmitted_counter.header_bytes += retransmission_packet.header_size();

    expected_retransmission_counter.num_packets += 1;
    expected_retransmission_counter.payload_bytes += retransmission_packet.payload_size();
    expected_retransmission_counter.header_bytes += retransmission_packet.header_size();

    EXPECT_CALL(
        stream_data_counters_observer_,
        OnStreamDataCountersUpdated(AllOf(Field(&RtpStreamDataCounters::transmitted,
                                                expected_transmitted_counter),
                                            Field(&RtpStreamDataCounters::retransmitted,
                                                expected_retransmission_counter),
                                            Field(&RtpStreamDataCounters::fec, kEmptyCounter)),
                                    kSsrc));
    sender->SendPacket(std::move(retransmission_packet));
    clock_.AdvanceTimeMs(10);

    // Send a padding packet.
    auto padding_packet = BuildRtpPacket();
    padding_packet.set_packet_type(RtpPacketType::PADDING);
    padding_packet.SetPadding(224);
    expected_transmitted_counter.num_packets += 1;
    expected_transmitted_counter.padding_bytes += padding_packet.padding_size();
    expected_transmitted_counter.header_bytes += padding_packet.header_size();

    EXPECT_CALL(
        stream_data_counters_observer_,
        OnStreamDataCountersUpdated(AllOf(Field(&RtpStreamDataCounters::transmitted,
                                                expected_transmitted_counter),
                                        Field(&RtpStreamDataCounters::retransmitted,
                                                expected_retransmission_counter),
                                        Field(&RtpStreamDataCounters::fec, kEmptyCounter)),
                                    kSsrc));
    sender->SendPacket(std::move(padding_packet));
    clock_.AdvanceTimeMs(10);
}

MY_TEST_P(RtpPacketEgresserTest, StreamDataCountersCallbacksFec) {
    auto sender = CreateRtpPacketEgresser();

    const RtpPacketCounter kEmptyCounter;
    RtpPacketCounter expected_transmitted_counter;
    RtpPacketCounter expected_fec_counter;

    // Send a media packet.
    auto media_packet = BuildRtpPacket();
    media_packet.SetPayloadSize(600);
    expected_transmitted_counter.num_packets += 1;
    expected_transmitted_counter.payload_bytes += media_packet.payload_size();
    expected_transmitted_counter.header_bytes += media_packet.header_size();

    EXPECT_CALL(
        stream_data_counters_observer_,
        OnStreamDataCountersUpdated(
            AllOf(Field(&RtpStreamDataCounters::transmitted,
                        expected_transmitted_counter),
                  Field(&RtpStreamDataCounters::retransmitted, kEmptyCounter),
                  Field(&RtpStreamDataCounters::fec, expected_fec_counter)),
            kSsrc));
    sender->SendPacket(std::move(media_packet));
    clock_.AdvanceTimeMs(10);

    // Send and FEC packet. FEC is counted into both transmitted and FEC packet
    // statistics.
    auto fec_packet = BuildRtpPacket();
    fec_packet.set_packet_type(RtpPacketType::FEC);
    fec_packet.SetPayloadSize(6);
    expected_transmitted_counter.num_packets += 1;
    expected_transmitted_counter.payload_bytes += fec_packet.payload_size();
    expected_transmitted_counter.header_bytes += fec_packet.header_size();

    expected_fec_counter.num_packets += 1;
    expected_fec_counter.payload_bytes += fec_packet.payload_size();
    expected_fec_counter.header_bytes += fec_packet.header_size();

    EXPECT_CALL(
        stream_data_counters_observer_,
        OnStreamDataCountersUpdated(
            AllOf(Field(&RtpStreamDataCounters::transmitted,
                        expected_transmitted_counter),
                  Field(&RtpStreamDataCounters::retransmitted, kEmptyCounter),
                  Field(&RtpStreamDataCounters::fec, expected_fec_counter)),
            kSsrc));
    sender->SendPacket(std::move(fec_packet));
    clock_.AdvanceTimeMs(10);
}

MY_TEST_P(RtpPacketEgresserTest, UpdatesDataCounters) {
    auto sender = CreateRtpPacketEgresser();

    const RtpPacketCounter kEmptyCounter;

    // Send a media packet.
    auto media_packet = BuildRtpPacket();
    media_packet.SetPayloadSize(6);
    sender->SendPacket(media_packet);
    clock_.AdvanceTime(TimeDelta::Zero());

    // Send an RTX retransmission packet.
    auto rtx_packet = BuildRtpPacket();
    rtx_packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    rtx_packet.set_ssrc(kRtxSsrc);
    rtx_packet.SetPayloadSize(7);
    rtx_packet.set_retransmitted_sequence_number(media_packet.sequence_number());
    sender->SendPacket(rtx_packet);
    clock_.AdvanceTime(TimeDelta::Zero());

    RtpStreamDataCounters rtp_stats = sender->GetRtpStreamDataCounter();
    RtpStreamDataCounters rtx_stats = sender->GetRtxStreamDataCounter();

    EXPECT_EQ(rtp_stats.transmitted.num_packets, 1u);
    EXPECT_EQ(rtp_stats.transmitted.payload_bytes, media_packet.payload_size());
    EXPECT_EQ(rtp_stats.transmitted.padding_bytes, media_packet.padding_size());
    EXPECT_EQ(rtp_stats.transmitted.header_bytes, media_packet.header_size());
    EXPECT_EQ(rtp_stats.retransmitted, kEmptyCounter);
    EXPECT_EQ(rtp_stats.fec, kEmptyCounter);

    // Retransmissions are counted both into transmitted and retransmitted
    // packet counts.
    EXPECT_EQ(rtx_stats.transmitted.num_packets, 1u);
    EXPECT_EQ(rtx_stats.transmitted.payload_bytes, rtx_packet.payload_size());
    EXPECT_EQ(rtx_stats.transmitted.padding_bytes, rtx_packet.padding_size());
    EXPECT_EQ(rtx_stats.transmitted.header_bytes, rtx_packet.header_size());
    EXPECT_EQ(rtx_stats.retransmitted, rtx_stats.transmitted);
    EXPECT_EQ(rtx_stats.fec, kEmptyCounter);
}

MY_TEST_P(RtpPacketEgresserTest, SendPacketUpdatesStats) {
    const size_t kPayloadSize = 1000;
   
    UlpFecGenerator ulp_fec_generator(98, 99);
    auto config = DefaultConfig();
    config.fec_generator = &ulp_fec_generator;
    auto sender = std::make_unique<RtpPacketEgresser>(config, &packet_history_);

    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                        rtp::TransportSequenceNumber::kUri);

    const int64_t capture_time_ms = clock_.now_ms();

    auto video_packet = BuildRtpPacket();
    // NOTE: Sets extension before setting payload size.
    video_packet.SetExtension<rtp::TransportSequenceNumber>(1);
    video_packet.set_packet_type(RtpPacketType::VIDEO);
    video_packet.SetPayloadSize(kPayloadSize);

    auto rtx_packet = BuildRtpPacket();
    rtx_packet.SetExtension<rtp::TransportSequenceNumber>(2);
    rtx_packet.set_ssrc(kRtxSsrc);
    rtx_packet.set_packet_type(RtpPacketType::RETRANSMISSION);
    rtx_packet.set_retransmitted_sequence_number(video_packet.sequence_number());
    rtx_packet.SetPayloadSize(kPayloadSize);
    
    auto fec_packet = BuildRtpPacket();
    fec_packet.SetExtension<rtp::TransportSequenceNumber>(3);
    // UlpFec packets share the same stream with meida packets.
    fec_packet.set_ssrc(kSsrc);
    fec_packet.set_packet_type(RtpPacketType::FEC);
    fec_packet.SetPayloadSize(kPayloadSize);

    const int64_t kDiffMs = 25;
    clock_.AdvanceTimeMs(kDiffMs);

    EXPECT_CALL(send_delay_observer_,
                OnSendDelayUpdated(kDiffMs, kDiffMs, kDiffMs, kSsrc));
    EXPECT_CALL(send_delay_observer_,
                OnSendDelayUpdated(kDiffMs, kDiffMs, 2 * kDiffMs, kSsrc));

    EXPECT_CALL(send_packet_observer_, 
                OnSendPacket(1, capture_time_ms, kSsrc));

    sender->SendPacket(std::move(video_packet));

    // Send packet observer not called for padding/retransmissions.
    EXPECT_CALL(send_packet_observer_, OnSendPacket(2, _, _)).Times(0);
    sender->SendPacket(std::move(rtx_packet));

    EXPECT_CALL(send_packet_observer_,
                OnSendPacket(3, capture_time_ms, kSsrc));
    sender->SendPacket(std::move(fec_packet));

    clock_.AdvanceTimeMs(0);
    RtpStreamDataCounters rtp_stats = sender->GetRtpStreamDataCounter();
    RtpStreamDataCounters rtx_stats = sender->GetRtxStreamDataCounter();
    EXPECT_EQ(rtp_stats.transmitted.num_packets, 2u);
    EXPECT_EQ(rtp_stats.fec.num_packets, 1u);
    EXPECT_EQ(rtx_stats.retransmitted.num_packets, 1u);
}

MY_TEST_P(RtpPacketEgresserTest, TransportFeedbackObserverWithRetransmission) {
    const uint16_t kTransportSequenceNumber = 17;
    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                        rtp::TransportSequenceNumber::kUri);
    auto retransmission = BuildRtpPacket();
    retransmission.set_packet_type(RtpPacketType::RETRANSMISSION);
    retransmission.SetExtension<rtp::TransportSequenceNumber>(kTransportSequenceNumber);

    uint16_t retransmitted_seq = retransmission.sequence_number() - 2;
    retransmission.set_retransmitted_sequence_number(retransmitted_seq);

    auto sender = CreateRtpPacketEgresser();
    EXPECT_CALL(
        transport_feedback_observer_,
        OnAddPacket(AllOf(
            Field(&RtpTransportFeedback::ssrc, kSsrc),
            Field(&RtpTransportFeedback::retransmitted_seq_num, retransmitted_seq),
            Field(&RtpTransportFeedback::packet_id, kTransportSequenceNumber))));
    sender->SendPacket(std::move(retransmission));
}

MY_TEST_P(RtpPacketEgresserTest, TransportFeedbackObserverWithRtxRetransmission) {
    const uint16_t kTransportSequenceNumber = 17;
    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                        rtp::TransportSequenceNumber::kUri);
    auto retransmission = BuildRtpPacket();
    retransmission.set_packet_type(RtpPacketType::RETRANSMISSION);
    retransmission.SetExtension<rtp::TransportSequenceNumber>(kTransportSequenceNumber);
    retransmission.set_ssrc(kRtxSsrc);
    uint16_t retransmitted_seq = retransmission.sequence_number() - 2;
    retransmission.set_retransmitted_sequence_number(retransmitted_seq);

    auto sender = CreateRtpPacketEgresser();
    EXPECT_CALL(
        transport_feedback_observer_,
        OnAddPacket(AllOf(
            Field(&RtpTransportFeedback::ssrc, kRtxSsrc),
            Field(&RtpTransportFeedback::media_ssrc, kSsrc),
            Field(&RtpTransportFeedback::retransmitted_seq_num, retransmitted_seq),
            Field(&RtpTransportFeedback::packet_id, kTransportSequenceNumber))));
    sender->SendPacket(std::move(retransmission));
}

MY_TEST_P(RtpPacketEgresserTest, TransportFeedbackObserverRtxPadding) {
    const uint16_t kTransportSequenceNumber = 17;
    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                        rtp::TransportSequenceNumber::kUri);
    auto rtx_padding = BuildRtpPacket();
    rtx_padding.set_packet_type(RtpPacketType::PADDING);
    rtx_padding.SetExtension<rtp::TransportSequenceNumber>(kTransportSequenceNumber);
    rtx_padding.SetPadding(222);
    rtx_padding.set_ssrc(kRtxSsrc);
  
    auto sender = CreateRtpPacketEgresser();
    EXPECT_CALL(
        transport_feedback_observer_,
        OnAddPacket(AllOf(
            Field(&RtpTransportFeedback::ssrc, kRtxSsrc),
            Field(&RtpTransportFeedback::media_ssrc, std::nullopt),
            Field(&RtpTransportFeedback::packet_id, kTransportSequenceNumber))));
    sender->SendPacket(std::move(rtx_padding));
}

MY_TEST_P(RtpPacketEgresserTest, TransportFeedbackObserverFEC) {
    const uint16_t kTransportSequenceNumber = 17;
    header_extension_map_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                        rtp::TransportSequenceNumber::kUri);
    auto fec = BuildRtpPacket();
    fec.set_packet_type(RtpPacketType::FEC);
    fec.SetExtension<rtp::TransportSequenceNumber>(kTransportSequenceNumber);
    fec.set_ssrc(kFlexFecSsrc);
    
    FlexfecGenerator flex_fec_generator(98, kFlexFecSsrc, kSsrc);
    auto config = DefaultConfig();
    config.fec_generator = &flex_fec_generator;
    auto sender = std::make_unique<RtpPacketEgresser>(config, &packet_history_);

    EXPECT_CALL(
        transport_feedback_observer_,
        OnAddPacket(AllOf(
            Field(&RtpTransportFeedback::ssrc, kFlexFecSsrc),
            Field(&RtpTransportFeedback::media_ssrc, std::nullopt),
            Field(&RtpTransportFeedback::packet_id, kTransportSequenceNumber))));
    sender->SendPacket(std::move(fec));
}

} // namespace test
} // namespace naivertc
