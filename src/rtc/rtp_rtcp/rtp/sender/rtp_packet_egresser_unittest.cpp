#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
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
    SendTransportImpl() 
        : total_bytes_sent_(0) {}

    bool SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        total_bytes_sent_ += packet.size();
        RtpPacketReceived recv_packet;
        EXPECT_TRUE(recv_packet.Parse(std::move(packet)));
        last_recv_packet_.emplace(std::move(recv_packet));
        return true;
    }

    bool SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) override {
        RTC_NOTREACHED();
    }

private:
    int64_t total_bytes_sent_;
    std::optional<RtpPacketReceived> last_recv_packet_;
};

} // namespace

class T(RtpPacketEgresserTest) : public ::testing::TestWithParam<bool> {
protected:
    T(RtpPacketEgresserTest)() 
        : clock_(123456),
          send_transport_(),
          packet_history_(&clock_, /*enable_rtx_padding_prioritization=*/true),
          seq_num_(kStartSequenceNumber) {};

    std::unique_ptr<RtpPacketEgresser> CreateRtpSenderEgresser() {
        return std::make_unique<RtpPacketEgresser>(DefaultConfig(), &packet_history_, nullptr);
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
        RtpPacketToSend packet(&header_extension_mgr_);
        packet.set_ssrc(kSsrc);
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
    rtp::HeaderExtensionManager header_extension_mgr_;
    uint16_t seq_num_;
};

MY_INSTANTIATE_TEST_SUITE_P(WithOrWithoutOverhead, RtpPacketEgresserTest, ::testing::Bool());

MY_TEST_P(RtpPacketEgresserTest, PacketSendStatsObserverGetsCorrectByteCount) {
    const size_t kRtpOverheadBytesPerPacket = 12 + 8;
    const size_t kPayloadSize = 1400;
    const uint16_t kTransportSeqNum = 17;

    header_extension_mgr_.RegisterByUri(kTransportSequenceNumberExtensionId, 
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

    auto sender = CreateRtpSenderEgresser();
    sender->SendPacket(std::move(packet));
}

MY_TEST_P(RtpPacketEgresserTest, OnSendDelayUpdated) {
    auto sender = CreateRtpSenderEgresser();

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
    auto sender = CreateRtpSenderEgresser();

    header_extension_mgr_.RegisterByUri(kTransportSequenceNumberExtensionId, rtp::TransportSequenceNumber::kUri);

    const uint16_t kTransportSeqNum = 123;
    const int64_t capture_time_ms = clock_.now_ms();
    EXPECT_CALL(send_packet_observer_,
                OnSendPacket(kTransportSeqNum, capture_time_ms, kSsrc));
    auto packet = BuildRtpPacket(/*marker=*/true, capture_time_ms);
    packet.SetExtension<rtp::TransportSequenceNumber>(kTransportSeqNum);
    sender->SendPacket(std::move(packet));
}

MY_TEST_P(RtpPacketEgresserTest, OnSendPacketNotUpdatedForRetransmission) {
    auto sender = CreateRtpSenderEgresser();

    header_extension_mgr_.RegisterByUri(kTransportSequenceNumberExtensionId, rtp::TransportSequenceNumber::kUri);

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

    auto sender = CreateRtpSenderEgresser();
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
    auto sender = CreateRtpSenderEgresser();

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

} // namespace test
} // namespace naivertc
