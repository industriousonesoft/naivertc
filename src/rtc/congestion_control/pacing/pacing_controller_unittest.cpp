#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "common/utils_random.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr DataRate kFirstClusterBitrate = DataRate::KilobitsPerSec(900);
constexpr DataRate kSecondClusterBitrate = DataRate::KilobitsPerSec(1800);

constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis(500);

// The error stems from truncating the time interval of probe packets to integer
// values. This results in probing slightly higher than the target bitrate.
// For 1.8 Mbps, this comes to be about 120 kbps with 1200 probe packets.
constexpr DataRate kProbingErrorMargin = DataRate::KilobitsPerSec(150);

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 23456;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr uint32_t kPaddingSsrc = kVideoSsrc;
constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(800);

RtpPacketToSend BuildPacket(RtpPacketType type,
                            uint32_t ssrc,
                            uint16_t seq_num,
                            int64_t capture_time_ms,
                            size_t payload_size) {
    auto packet = RtpPacketToSend(nullptr);
    packet.set_packet_type(type);
    packet.set_ssrc(ssrc);
    packet.set_sequence_number(seq_num);
    packet.set_capture_time_ms(capture_time_ms);
    packet.set_payload_size(payload_size);
    return packet;
}

// MediaStream
struct MediaStream {
    const RtpPacketType packet_type;
    const uint32_t ssrc;
    const size_t packet_size;
    uint16_t seq_num;
};

MediaStream kAudioStream {RtpPacketType::AUDIO, kAudioSsrc, 100, 1234};
MediaStream kVideoStream {RtpPacketType::VIDEO, kVideoSsrc, 1000, 1234};

} // namespace

// MockPacingPacketSender
class MockPacingPacketSender : public PacingController::PacketSender {
public:
    void SendPacket(RtpPacketToSend packet, 
                    const PacedPacketInfo& pacing_info) override {
        if (packet.packet_type() != RtpPacketType::PADDING) {
            ++media_packets_sent_;
        }
        // Account bytes both the meida packets and padding packets.
        total_bytes_sent_ += packet.payload_size();
        last_pacing_info_ = pacing_info;
        // Send packets
        SendPacket(packet.packet_type(),
                   packet.ssrc(),
                   packet.sequence_number(),
                   packet.capture_time_ms(),
                   packet.payload_size());
        // Send probe packets
        if (pacing_info.probe_cluster.has_value()) {
            SendProbe(packet.packet_type(), packet.ssrc(), pacing_info.probe_cluster->id);
        }
        
    }

    std::vector<RtpPacketToSend> GeneratePadding(size_t target_size) override {
        // From RTPSender:
        // Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
        const size_t kMaxPadding = 224;
        target_size = SendPadding(target_size);
        std::vector<RtpPacketToSend> packets;
        while (target_size > 0) {
            RtpPacketToSend packet(nullptr);
            size_t padding_size = std::min(kMaxPadding, target_size);
            packet.set_ssrc(kPaddingSsrc);
            packet.set_packet_type(RtpPacketType::PADDING);
            packet.set_payload_size(padding_size);
            packets.emplace_back(std::move(packet));

            padding_sent_ += padding_size;
            target_size -= padding_size;

            SendPaddingPacket(padding_size);
        }
        return packets;
    }

    MOCK_METHOD(void, 
                SendPacket, 
                (RtpPacketType packet_type,
                uint32_t ssrc,
                uint16_t seq_num, 
                int64_t capture_time_ms, 
                size_t payload_size));
    MOCK_METHOD(std::vector<RtpPacketToSend>,
                FetchFecPackets,
                (),
                (override));
    MOCK_METHOD(size_t, SendPadding, (size_t target_size));
    MOCK_METHOD(void, SendPaddingPacket, (size_t packet_size));
    MOCK_METHOD(void, SendProbe, (RtpPacketType, uint32_t ssrc, int probe_cluster_id));

    size_t padding_sent() const { return padding_sent_; }
    size_t total_bytes_sent() const { return total_bytes_sent_; }
    size_t media_packets_sent() const { return media_packets_sent_; }
    PacedPacketInfo last_pacing_info() const { return last_pacing_info_; }

private:
    size_t padding_sent_;
    size_t total_bytes_sent_;
    size_t media_packets_sent_;
    PacedPacketInfo last_pacing_info_;
};

// PacingControllerTest
class T(PacingControllerTest) : public ::testing::Test {
public:
    T(PacingControllerTest)() : clock_(1000'000) {}

    void SetUp() override {
        pacing_config_.clock = &clock_;
        pacing_config_.packet_sender = &packet_sender_;
        pacer_ = std::make_unique<PacingController>(pacing_config_);

        pacer_->SetPacingBitrate(kTargetRate * PacingController::kDefaultPaceMultiplier, DataRate::Zero());
    }

    bool EnqueuePacketFrom(MediaStream& stream) {
        return pacer_->EnqueuePacket(BuildPacket(stream.packet_type, 
                                                 stream.ssrc, 
                                                 stream.seq_num++, 
                                                 clock_.now_ms(), 
                                                 stream.packet_size));
    }

    bool EnqueueAndVerifyPacket(RtpPacketType type,
                                uint32_t ssrc,
                                uint16_t seq_num,
                                int64_t capture_time_ms,
                                size_t payload_size) {
        EXPECT_CALL(packet_sender_, SendPacket(type, ssrc, seq_num, capture_time_ms, payload_size));
        return EnqueuePacket(type, ssrc, seq_num, capture_time_ms, payload_size);
    }

    bool EnqueuePacket(RtpPacketType type,
                       uint32_t ssrc,
                       uint16_t seq_num,
                       int64_t capture_time_ms,
                       size_t payload_size) {
        return pacer_->EnqueuePacket(BuildPacket(type, ssrc, seq_num, capture_time_ms, payload_size));
    }

    TimeDelta TimeUntilNextProcess() {
        auto now = clock_.CurrentTime();
        auto next_send_time = pacer_->NextSendTime();
        return std::max(TimeDelta::Zero(), next_send_time - now);
    }

    void ProcessNext() {
        clock_.AdvanceTime(TimeUntilNextProcess());
        pacer_->ProcessPackets();
    }

protected:
    SimulatedClock clock_;
    PacingController::Configuration pacing_config_;
    std::unique_ptr<PacingController> pacer_;
    ::testing::NiceMock<MockPacingPacketSender> packet_sender_;
};

MY_TEST_F(PacingControllerTest, DISABLED_DefaultNoPaddingInSilence) {
    pacer_->SetPacingBitrate(kTargetRate, DataRate::Zero());
    // Video packet to reset last send time and provide padding data.
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();

    // Should not trigger sending of padding even if waiting 500 ms 
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    // Advance 500 ms since last process.
    clock_.AdvanceTimeMs(500);
    pacer_->ProcessPackets();
}

MY_TEST_F(PacingControllerTest, DISABLED_EnablePaddingInSilence) {
    pacing_config_.pacing_setting.send_padding_if_silent = true;
    SetUp();
    pacer_->SetPacingBitrate(kTargetRate, DataRate::Zero());
    // Video packet to reset last send time and provide padding data.
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(2);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();

    // Waiting 500 ms should trigger sending of padding.
    EXPECT_CALL(packet_sender_, SendPadding).WillOnce(Return(1));
    // Advance 500 ms since last process.
    clock_.AdvanceTimeMs(500);
    pacer_->ProcessPackets();
}

MY_TEST_F(PacingControllerTest, DISABLED_EnablePacingAudio) {
    pacing_config_.pacing_setting.pacing_audio = true;
    SetUp();
    pacer_->SetPacingBitrate(kTargetRate, DataRate::Zero());

    auto congestion_window = kVideoStream.packet_size - 100;
    pacer_->SetCongestionWindow(congestion_window);
    pacer_->OnInflightBytes(0);
    EXPECT_FALSE(pacer_->IsCongested());

    // Video packet will fill congestion window.
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();
    EXPECT_TRUE(pacer_->IsCongested());

    // Audio packet will be blocked due to congestion.
    EnqueuePacketFrom(kAudioStream);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::AUDIO, kAudioSsrc, _, _, _)).Times(0);
    // We will send padding as heartbeat when congested.
    EXPECT_CALL(packet_sender_, SendPadding(1)).Times(2);
    ProcessNext();
    ProcessNext();

    // Audio packet unblocked when congestion window clear.
    pacer_->OnInflightBytes(congestion_window - 1);
    EXPECT_FALSE(pacer_->IsCongested());
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();
}

MY_TEST_F(PacingControllerTest, DISABLED_DefaultNotPacingAudio) {
    pacer_->SetPacingBitrate(kTargetRate, DataRate::Zero());

    auto congestion_window = kVideoStream.packet_size - 100;
    pacer_->SetCongestionWindow(congestion_window);
    pacer_->OnInflightBytes(0);
    EXPECT_FALSE(pacer_->IsCongested());

    // Video packet fills congestion window.
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();

    // Audio not blocked due to congestion.
    EnqueuePacketFrom(kAudioStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();
}

MY_TEST_F(PacingControllerTest, DISABLED_DefaultDebtNotAffectAudio) {
    pacer_->SetPacingBitrate(kTargetRate, DataRate::Zero());

    // Video fills budget for following process periods,
    // as the media debt can't be paid off by one process。
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();

    // Audio not blocked due to budget limit.
    EnqueuePacketFrom(kAudioStream);
    Timestamp wait_start_time = clock_.CurrentTime();
    Timestamp wait_end_time = Timestamp::MinusInfinity();
    EXPECT_CALL(packet_sender_, SendPacket).WillOnce([&](RtpPacketType packet_type,
                                                         uint32_t ssrc,
                                                         uint16_t seq_num, 
                                                         int64_t capture_time_ms, 
                                                         size_t payload_size){
        // The next packet MUST be audio.
        ASSERT_EQ(packet_type, RtpPacketType::AUDIO);
        wait_end_time = clock_.CurrentTime();
    });
    while (wait_end_time.IsInfinite()) {
        ProcessNext();
    }
    // 音频发送不需要等待视频发送完，其发送时间为入队列的时间。
    EXPECT_EQ(wait_start_time, wait_end_time);
}

MY_TEST_F(PacingControllerTest, DISABLED_DebtAffectsAudio) {
    pacing_config_.pacing_setting.pacing_audio = true;
    SetUp();
    EXPECT_FALSE(pacer_->IsCongested());

    auto pacing_bitrate = kTargetRate;
    pacer_->SetPacingBitrate(pacing_bitrate, DataRate::Zero());

    // Video fills budget for following process periods,
    // as the media debt can't be paid off by one process。
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();
    EXPECT_FALSE(pacer_->IsCongested());

    // Audio not blocked due to budget limit.
    EnqueuePacketFrom(kAudioStream);
    Timestamp wait_start_time = clock_.CurrentTime();
    Timestamp wait_end_time = Timestamp::MinusInfinity();
    EXPECT_CALL(packet_sender_, SendPacket).WillOnce([&](RtpPacketType packet_type,
                                                         uint32_t ssrc,
                                                         uint16_t seq_num, 
                                                         int64_t capture_time_ms, 
                                                         size_t payload_size){
        // The next packet MUST be audio.
        ASSERT_EQ(packet_type, RtpPacketType::AUDIO);
        wait_end_time = clock_.CurrentTime();
    });
    while (wait_end_time.IsInfinite()) {
        ProcessNext();
    }
    
    const TimeDelta elapsed_time = wait_end_time - wait_start_time;
    // 音频发送会受到视频影响，必须等视频发送完才能发送。
    EXPECT_GT(elapsed_time, TimeDelta::Zero());
    // 等待视频发送完的时间
    const TimeDelta expected_wait_time = kVideoStream.packet_size / pacing_bitrate;
    EXPECT_LT(((wait_end_time - wait_start_time) - expected_wait_time).Abs(), PacingController::kMaxEarlyProbeProcessing);

}

MY_TEST_F(PacingControllerTest, DISABLED_FirstSentPacketTimeIsSet) {
    // No packet sent.
    EXPECT_FALSE(pacer_->first_sent_packet_time().has_value());

    const Timestamp start_time = clock_.CurrentTime();
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(EnqueuePacketFrom(kVideoStream));
        EXPECT_FALSE(pacer_->IsCongested());
        ProcessNext();
    }
    EXPECT_EQ(start_time, pacer_->first_sent_packet_time());
}

MY_TEST_F(PacingControllerTest, DISABLED_QueuePacket) {
    const size_t kPacketSize = 250;
    // Divide one second into 200 intervals, and each interval is 5ms.
    const TimeDelta kSendInterval = TimeDelta::Millis(5);
    // The packets we can send per second.
    const size_t kPacketsPerSec = kTargetRate.bps() * PacingController::kDefaultPaceMultiplier / (8 * kPacketSize);
    const size_t kPacketsPerInterval = static_cast<size_t>(kPacketsPerSec * kSendInterval.seconds<double>());

    uint16_t seq_num = 100;
    // Send packets during a send inteval (5ms).
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
   
    // Enqueue one extra packet
    int64_t queue_packet_time = clock_.now_ms();
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num, queue_packet_time, kPacketSize);
    EXPECT_EQ(kPacketsPerInterval + 1, pacer_->NumQueuedPackets());

    // Send packets until the intial kPacketsPerInterval packets are done.
    auto start_time = clock_.CurrentTime();
    while (pacer_->NumQueuedPackets() > 1) {
        ProcessNext();
    }
    EXPECT_LT(clock_.CurrentTime() - start_time, kSendInterval);
    EXPECT_EQ(1u, pacer_->NumQueuedPackets());

    // Proceed till last packet can be sent.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num, queue_packet_time, kPacketSize)).Times(1);
    ProcessNext();

    EXPECT_GE(clock_.CurrentTime() - start_time, kSendInterval);
    EXPECT_EQ(0u, pacer_->NumQueuedPackets());
}

MY_TEST_F(PacingControllerTest, DISABLED_PaceQueuedPackets) {
    const size_t kPacketSize = 250;
    // Divide one second into 200 intervals, and each interval is 5ms.
    const TimeDelta kSendInterval = TimeDelta::Millis(5);
    // The packets we can send per second.
    const size_t kPacketsPerSec = kTargetRate.bps() * PacingController::kDefaultPaceMultiplier / (8 * kPacketSize);
    const size_t kPacketsPerInterval = static_cast<size_t>(kPacketsPerSec * kSendInterval.seconds<double>());

    uint16_t seq_num = 100;
    // Send packets during a send inteval (5ms).
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }
    // Enqueue more packets
    for (size_t i = 0; i < kPacketsPerInterval * 10; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }
    EXPECT_EQ(kPacketsPerInterval + kPacketsPerInterval * 10, pacer_->NumQueuedPackets());

    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    auto start_time = clock_.CurrentTime();
    while (pacer_->NumQueuedPackets() > kPacketsPerInterval * 10) {
        ProcessNext();
    }
    EXPECT_LT(clock_.CurrentTime() - start_time, kSendInterval);
    EXPECT_EQ(kPacketsPerInterval * 10, pacer_->NumQueuedPackets());

    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, kPacketSize)).Times(pacer_->NumQueuedPackets());
    const TimeDelta expected_pacing_time = pacer_->NumQueuedPackets() * kPacketSize / (kTargetRate * PacingController::kDefaultPaceMultiplier);
    start_time = clock_.CurrentTime();
    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
    const TimeDelta actual_pacing_time = clock_.CurrentTime() - start_time;
    EXPECT_LT((actual_pacing_time - expected_pacing_time).Abs(), PacingController::kMaxEarlyProbeProcessing);
}

MY_TEST_F(PacingControllerTest, DISABLED_RepeatedRetransmissionAllowed) {
    // Send one packet, then two retransmissions of that packet.
    for (size_t i = 0; i < 3; ++i) {
        bool is_retransmission = (i != 0);
        EnqueueAndVerifyPacket(is_retransmission ? RtpPacketType::RETRANSMISSION
                                              : RtpPacketType::VIDEO,
                                kVideoSsrc, 
                                222,
                                clock_.now_ms(),
                                250);
        clock_.AdvanceTimeMs(5);
    }
    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_CanQueuePacketsWithSameSequeueNumberOnDifferentSsrcs) {
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, 123, clock_.now_ms(), 1000);
    // Expect packet on second ssrc to be queued and sent as well.
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc + 1, 123, clock_.now_ms(), 1000);

    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, Padding) {
    pacer_->SetPacingBitrate(kTargetRate * PacingController::kDefaultPaceMultiplier, kTargetRate);

    const size_t kPacketSize = 250;
    const size_t kPacketToSend = 20;
    uint16_t seq_num = 100;
    for (size_t i = 0; i < kPacketToSend; ++i) {
        EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }
    
    const auto expected_pacing_time = pacer_->NumQueuedPackets() * kPacketSize / (kTargetRate * PacingController::kDefaultPaceMultiplier);
    auto start_time = clock_.CurrentTime();
    // Only the media packets should be sent.
    // EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
    const auto actual_pacing_time = clock_.CurrentTime() - start_time;
    EXPECT_LE((actual_pacing_time - expected_pacing_time).Abs(), PacingController::kMaxEarlyProbeProcessing) << actual_pacing_time.ms() << " - "
                                                                                                             << expected_pacing_time.ms();
    // Pacing media happens at 2.5x, but padding was configured with 1.0x factor.
    // We have to wait until the padding debt is gone before we start sending padding.
    const auto time_to_padding_debe_free = (expected_pacing_time * PacingController::kDefaultPaceMultiplier) - actual_pacing_time;
    // Pay off the padding debt.
    clock_.AdvanceTime(time_to_padding_debe_free);
    pacer_->ProcessPackets();

    // Send 10 padding packets.
    const size_t kPaddingPacketsToSend = 10;
    size_t padding_sent = 0;
    size_t padding_packets_sent = 0;
    auto first_send_time = Timestamp::MinusInfinity();
    auto last_send_time = Timestamp::MinusInfinity();

    EXPECT_CALL(packet_sender_, SendPadding).WillRepeatedly([&](size_t padding_size){
        return padding_size;
    });
    EXPECT_CALL(packet_sender_, SendPaddingPacket).Times(kPaddingPacketsToSend)
                                                   .WillRepeatedly([&](size_t packet_size){
        ++padding_packets_sent;
        GTEST_COUT << "padding_packets_sent=" << padding_packets_sent << std::endl;
        if (padding_packets_sent < kPaddingPacketsToSend) {
            // Don't count bytes of last packet, instead just
            // use this as the time the last packet finished
            // sending.
            padding_sent += packet_size;
        }
        if (first_send_time.IsInfinite()) {
            first_send_time = clock_.CurrentTime();
        } else {
            last_send_time = clock_.CurrentTime();
        }
    });
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(kPaddingPacketsToSend);
    while (padding_packets_sent < kPaddingPacketsToSend) {
        ProcessNext();
    }
    // Verify bitrate of padding.
    auto padding_duration = last_send_time - first_send_time;
    auto padding_bitrate = padding_sent / padding_duration;
    EXPECT_EQ(padding_bitrate, kTargetRate);
}

MY_TEST_F(PacingControllerTest, DISABLED_NoPaddingBeforeNormalPacket) {
    pacer_->SetPacingBitrate(kTargetRate * PacingController::kDefaultPaceMultiplier, kTargetRate);

    EXPECT_CALL(packet_sender_, SendPadding).Times(0);

    ProcessNext();
    ProcessNext();

    const size_t kPacketSize = 250;
    const size_t kPacketToSend = 20;
    uint16_t seq_num = 100;
    // Enqueue a normal packet
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);

    bool padding_sent = false;
    EXPECT_CALL(packet_sender_, SendPadding).WillOnce([&](size_t padding_size){
        padding_sent = true;
        return padding_size;
    });
    // Padding will be sent after sending normal packet.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    while (!padding_sent) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_VerifyAverageBitrateVerifyMediaPayload) {
    pacer_->SetPacingBitrate(kTargetRate * PacingController::kDefaultPaceMultiplier, DataRate::Zero());

    const TimeDelta kAveragingWindowSize = TimeDelta::Seconds(10);

    auto start_time = clock_.CurrentTime();
    uint32_t seq_num = 100;
    size_t media_bytes = 0;
    while (clock_.CurrentTime() - start_time < kAveragingWindowSize) {
        while (media_bytes < (kTargetRate * (clock_.CurrentTime() - start_time))) {
            size_t media_payload = utils::random::random(800, 1200); // [800, 1200]
            EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), media_payload);
            media_bytes += media_payload;
        }
        ProcessNext();
    }

    EXPECT_NEAR(kTargetRate.bps(), 
                (packet_sender_.total_bytes_sent() / kAveragingWindowSize).bps(), 
                (kTargetRate * 0.01 /* 1% error marging */).bps());

}

MY_TEST_F(PacingControllerTest, DISABLED_Priority) {
    const size_t kPacketSize = 250;
    // Divide one second into 200 intervals, and each interval is 5ms.
    const TimeDelta kSendInterval = TimeDelta::Millis(5);
    // The packets we can send per second.
    const size_t kPacketsPerSec = kTargetRate.bps() * PacingController::kDefaultPaceMultiplier / (8 * kPacketSize);
    const size_t kPacketsPerInterval = static_cast<size_t>(kPacketsPerSec * kSendInterval.seconds<double>());

    // Video packet takes lower priority
    uint16_t seq_num = 100;
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);

    // Retransmission packet takes normal priority
    // Send retransmission packets during a send inteval (5ms).
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueuePacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }

    // Video packet take high priority.
    EnqueuePacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);

    // Expect all high and normal priority to be sent out first.
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::AUDIO, kAudioSsrc, _, _, _)).Times(1);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, _, _, _)).Times(kPacketsPerInterval);

    // The video with lower prority will be left in queue.
    while (pacer_->NumQueuedPackets() > 1) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_RetransmissionPriority) {
    const size_t kPacketSize = 250;
    // Divide one second into 200 intervals, and each interval is 5ms.
    const TimeDelta kSendInterval = TimeDelta::Millis(5);
    // The packets we can send per second.
    const size_t kPacketsPerSec = kTargetRate.bps() * PacingController::kDefaultPaceMultiplier / (8 * kPacketSize);
    const size_t kPacketsPerInterval = static_cast<size_t>(kPacketsPerSec * kSendInterval.seconds<double>());

    // Video packet takes lower priority
    uint16_t seq_num = 100;
    const TimeDelta kRetransmissionCaptureDelay = TimeDelta::Millis(500);
    // Retransmission packet takes normal priority
    // Send retransmission packets during a send inteval (5ms).
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        EnqueuePacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, seq_num++, clock_.now_ms() + kRetransmissionCaptureDelay.ms(), kPacketSize);
    }
    EXPECT_EQ(kPacketsPerInterval * 2, pacer_->NumQueuedPackets());

    // Expect all retransmission to be sent out first
    // despite having a later capture time.
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, _)).Times(0);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, _, _, _)).Times(kPacketsPerInterval);

    while (pacer_->NumQueuedPackets() > kPacketsPerInterval) {
        ProcessNext();
    }
    EXPECT_EQ(kPacketsPerInterval, pacer_->NumQueuedPackets());

    // Expect all remaing to be sent.
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, _)).Times(kPacketsPerInterval);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, _, _, _)).Times(0);

    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
    EXPECT_EQ(0u, pacer_->NumQueuedPackets());
}

MY_TEST_F(PacingControllerTest, DISABLED_HighPriorityDoesntAffectDebt) {
    const size_t kPacketSize = 250;
    // Divide one second into 200 intervals, and each interval is 5ms.
    const TimeDelta kSendInterval = TimeDelta::Millis(5);
    // The packets we can send per second.
    const size_t kPacketsPerSec = kTargetRate.bps() * PacingController::kDefaultPaceMultiplier / (8 * kPacketSize);
    const size_t kPacketsPerInterval = static_cast<size_t>(kPacketsPerSec * kSendInterval.seconds<double>());

    // As high priority packets doesn't affect tht debt, we
    // should be able to send a high number of them at once.
    uint16_t seq_num = 100;
    const size_t kNumAudioPackets = 25;
    for (size_t i = 0; i < kNumAudioPackets; ++i) {
        EnqueuePacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }
    EXPECT_EQ(kNumAudioPackets, pacer_->NumQueuedPackets());
    // All the audio packet will sent at once.
    pacer_->ProcessPackets();
    EXPECT_EQ(0u, pacer_->NumQueuedPackets());

    // Low priority packtes does affect the debt.
    const TimeDelta kRetransmissionCaptureDelay = TimeDelta::Millis(500);
    // Send retransmission packets during a send inteval (5ms).
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }

    auto start_time = clock_.CurrentTime();
    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }

    // Measure pacing time, and expect only low-priority packtes to affect this.
    auto pacing_time = clock_.CurrentTime() - start_time;
    auto expected_pacing_time = (kPacketsPerInterval * kPacketSize) / (kTargetRate * PacingController::kDefaultPaceMultiplier);
    EXPECT_NEAR(pacing_time.ms(), expected_pacing_time.ms(), PacingController::kMaxEarlyProbeProcessing.ms());

}

MY_TEST_F(PacingControllerTest, DISABLED_SendsHeartbeatOnlyWhenCongested) {
    const size_t kPacketSize = 250;
    const size_t kCongestionWindow = kPacketSize * 10;

    pacer_->OnInflightBytes(0);
    pacer_->SetCongestionWindow(kCongestionWindow);

    uint16_t seq_num = 0;
    size_t sent_bytes = 0;
    while (sent_bytes < kCongestionWindow) {
        sent_bytes += kPacketSize;
        EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        ProcessNext();
    }

    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    EXPECT_CALL(packet_sender_, SendPacket).Times(0);

    size_t blocked_packets = 0;
    // Send a heartbeat every 500ms if congested.
    int64_t expected_time_until_padding = 500;
    while (expected_time_until_padding > 5) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        ++blocked_packets;
        clock_.AdvanceTimeMs(5);
        pacer_->ProcessPackets();
        expected_time_until_padding -= 5;
    }
    // Heartbeat packet with 1 padding byte.
    EXPECT_CALL(packet_sender_, SendPadding(1)).WillOnce(Return(1));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();
    EXPECT_EQ(blocked_packets, pacer_->NumQueuedPackets());
}

MY_TEST_F(PacingControllerTest, DISABLED_DoesNotAllowOveruseAfterCongestion) {
    const size_t kPacketSize = 1000;
    // The pacing bitrate is low enough that the budget should not allow
    // two packets to be sent in a row.
    // time_inflight_ms = 1000 * 8000 / 640'000 = 12.5 ms
    pacer_->SetPacingBitrate(DataRate::KilobitsPerSec(640), DataRate::Zero());
    
    // The congestion window is small enough (< packet size) to only let one packet through at a time.
    pacer_->SetCongestionWindow(800);
    pacer_->OnInflightBytes(0);
    EXPECT_FALSE(pacer_->IsCongested());

    uint16_t seq_num = 0;
    // Not yet budget limited or congested, packet is sent.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();
    EXPECT_TRUE(pacer_->IsCongested());

    // Packet will be blocked due to congestion.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket).Times(0);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();
    EXPECT_TRUE(pacer_->IsCongested());

    // Packet will be blocked due to congestion.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket).Times(0);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();
    EXPECT_TRUE(pacer_->IsCongested());

    // Congestion removed and budget has recovered, packet will be sent.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    pacer_->OnInflightBytes(100);
    EXPECT_FALSE(pacer_->IsCongested());
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();

    // Packet will be blocked due to new congestion.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket).Times(0);
    clock_.AdvanceTimeMs(5);
    pacer_->ProcessPackets();
    EXPECT_TRUE(pacer_->IsCongested());
}
    
MY_TEST_F(PacingControllerTest, DISABLED_ResumeSendingWhenCongestionEnds) {
    const size_t kPacketSize = 250;
    const size_t kCongestionCount = 10;
    const size_t kCongestionWindow = kPacketSize * kCongestionCount;
    const int64_t kCongetsionTimeMs = 1000;

    pacer_->OnInflightBytes(0);
    pacer_->SetCongestionWindow(kCongestionWindow);

    size_t sent_bytes = 0;
    uint16_t seq_num = 0;
    while (sent_bytes < kCongestionWindow) {
        sent_bytes += kPacketSize;
        EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        clock_.AdvanceTimeMs(5);
        pacer_->ProcessPackets();
    }

    EXPECT_CALL(packet_sender_, SendPacket).Times(0);
    size_t unacked_packets = 0;
    for (int duration = 0; duration < kCongetsionTimeMs; duration += 5) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        unacked_packets++;
        clock_.AdvanceTimeMs(5);
        pacer_->ProcessPackets();
    }

    // First mark half of the congested packets as cleared and make sure 
    // that just as many as sent.
    size_t acked_packets = kCongestionCount / 2;
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, _)).Times(acked_packets);
    pacer_->OnInflightBytes(kCongestionWindow - kPacketSize * acked_packets);
    EXPECT_FALSE(pacer_->IsCongested());
    for (int duration = 0; duration < kCongetsionTimeMs; duration += 5) {
        clock_.AdvanceTimeMs(5);
        pacer_->ProcessPackets();
    }
    unacked_packets -= acked_packets;
    EXPECT_TRUE(pacer_->IsCongested());
    
    // Second make sure all packets are sent if sent packets are continuously marked as acked.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, _)).Times(unacked_packets);
    for (int duration = 0; duration < kCongetsionTimeMs; duration += 5) {
        pacer_->OnInflightBytes(0);
        EXPECT_FALSE(pacer_->IsCongested());
        clock_.AdvanceTimeMs(5);
        pacer_->ProcessPackets();
    }
    EXPECT_FALSE(pacer_->IsCongested());
}

MY_TEST_F(PacingControllerTest, DISABLED_Pause) {
    const size_t kPacketSize = 250;
    // Divide one second into 200 intervals, and each interval is 5ms.
    const TimeDelta kSendInterval = TimeDelta::Millis(5);
    // The packets we can send per second.
    const size_t kPacketsPerSec = kTargetRate.bps() * PacingController::kDefaultPaceMultiplier / (8 * kPacketSize);
    const size_t kPacketsPerInterval = static_cast<size_t>(kPacketsPerSec * kSendInterval.seconds<double>());

    EXPECT_TRUE(pacer_->OldestPacketEnqueueTime().IsInfinite());

    pacer_->Pause();

    int64_t first_capture_time_ms = clock_.now_ms();
    uint16_t seq_num = 100;
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, first_capture_time_ms, kPacketSize);
        EnqueuePacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, seq_num++, first_capture_time_ms, kPacketSize);
        EnqueuePacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, first_capture_time_ms, kPacketSize);
    }
    clock_.AdvanceTimeMs(10'000); // 10s
    int64_t second_capture_time_ms = clock_.now_ms();
    for (size_t i = 0; i < kPacketsPerInterval; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, second_capture_time_ms, kPacketSize);
        EnqueuePacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, seq_num++, second_capture_time_ms, kPacketSize);
        EnqueuePacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, second_capture_time_ms, kPacketSize);
    }

    // Expect all packets to be queued.
    EXPECT_EQ(first_capture_time_ms, pacer_->OldestPacketEnqueueTime().ms());

    // Process Trigger heartbeat packet
    EXPECT_CALL(packet_sender_, SendPadding(1)).WillOnce(Return(1));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    pacer_->ProcessPackets();

    // Verify no packets sent for the rest of the paused process interval.
    auto expected_time_until_send = PacingController::kPausedProcessInterval;
    EXPECT_CALL(packet_sender_, SendPacket).Times(0);
    while (expected_time_until_send >= kSendInterval) {
        pacer_->ProcessPackets();
        clock_.AdvanceTime(kSendInterval);
        expected_time_until_send -= kSendInterval;
    }

    // A new heartbeat pakcet every paused process interval.
    EXPECT_CALL(packet_sender_, SendPadding(1)).WillOnce(Return(1));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    pacer_->ProcessPackets();

    // Expect high prio packets to come out first followed by normal
    // prio packets and low prio packets (all in capture order).
    {
        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::AUDIO, kAudioSsrc, _, first_capture_time_ms, _)).Times(kPacketsPerInterval);
        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::AUDIO, kAudioSsrc, _, second_capture_time_ms, _)).Times(kPacketsPerInterval);

        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, _, first_capture_time_ms, _)).Times(kPacketsPerInterval);
        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::RETRANSMISSION, kVideoRtxSsrc, _, second_capture_time_ms, _)).Times(kPacketsPerInterval);

        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, first_capture_time_ms, _)).Times(kPacketsPerInterval);
        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, second_capture_time_ms, _)).Times(kPacketsPerInterval);
    }
    pacer_->Resume();

    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
    EXPECT_TRUE(pacer_->OldestPacketEnqueueTime().IsInfinite());
}

MY_TEST_F(PacingControllerTest, DISABLED_InactiveFromStart) {
    pacer_->SetProbingEnabled(false);
    pacer_->SetPacingBitrate(kTargetRate * PacingController::kDefaultPaceMultiplier, kTargetRate);

    // No packets sent, there should be no heartbeat sent either.
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    EXPECT_CALL(packet_sender_, SendPacket).Times(0);
    pacer_->ProcessPackets();

    const auto start_time = clock_.CurrentTime();
    const auto time_margin = PacingController::kMaxEarlyProbeProcessing + TimeDelta::Micros(1);

    EXPECT_EQ(pacer_->NextSendTime() - start_time, PacingController::kPausedProcessInterval);
    clock_.AdvanceTime(PacingController::kPausedProcessInterval - time_margin);
    pacer_->ProcessPackets();
    // Not cause a process event
    EXPECT_EQ(pacer_->NextSendTime() - start_time, PacingController::kPausedProcessInterval);

    clock_.AdvanceTime(time_margin);
    pacer_->ProcessPackets();
    // Not cause a process event
    EXPECT_EQ(pacer_->NextSendTime() - start_time, 2 * PacingController::kPausedProcessInterval);
}

MY_TEST_F(PacingControllerTest, DISABLED_ExpectQueueTime) {
    const size_t kNumPackets = 60;
    const size_t kPacketSize = 1200;
    const DataRate kMaxBitrate = DataRate::BitsPerSec(30000 * PacingController::kDefaultPaceMultiplier);

    EXPECT_TRUE(pacer_->OldestPacketEnqueueTime().IsInfinite());

    pacer_->SetPacingBitrate(kMaxBitrate, DataRate::Zero());

    uint16_t seq_num = 100;
    for (size_t i = 0; i < kNumPackets; ++i) {
        EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }

    const TimeDelta queue_time = kNumPackets * kPacketSize / kMaxBitrate;
    EXPECT_EQ(queue_time, pacer_->ExpectedQueueTime());

    const auto start_time = clock_.CurrentTime();
    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
    EXPECT_EQ(TimeDelta::Zero(), pacer_->ExpectedQueueTime());
    auto actual_queue_time = clock_.CurrentTime() - start_time;

    // the actual queue time should not exceed max queue time limit.
    EXPECT_LT((actual_queue_time - PacingController::kMaxExpectedQueueTime).Abs(), kPacketSize / kMaxBitrate);

}

MY_TEST_F(PacingControllerTest, DISABLED_QueueTimeGrowsOverTime) {
    EXPECT_TRUE(pacer_->OldestPacketEnqueueTime().IsInfinite());

    uint16_t seq_num = 100;
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), 1200);

    clock_.AdvanceTimeMs(500);
    EXPECT_EQ(clock_.now_ms() - 500, pacer_->OldestPacketEnqueueTime().ms());

    pacer_->ProcessPackets();
    EXPECT_TRUE(pacer_->OldestPacketEnqueueTime().IsInfinite());
}

MY_TEST_F(PacingControllerTest, DISABLED_ProbingWithInsertedPackets) {
    const size_t kPacketSize = 1200;
    const DataRate kInitialBitrate = DataRate::KilobitsPerSec(300);

    pacer_->AddProbeCluster(0, kFirstClusterBitrate);
    pacer_->AddProbeCluster(1, kSecondClusterBitrate);

    pacer_->SetPacingBitrate(kInitialBitrate * PacingController::kDefaultPaceMultiplier, DataRate::Zero());

    uint16_t seq_num = 100;
    for (int i = 0; i < 10; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }

    auto start_time = clock_.CurrentTime();
    while (packet_sender_.media_packets_sent() < 5) {
        ProcessNext();
    }
    auto media_packets_sent = packet_sender_.media_packets_sent();
    auto elapsed_time = clock_.CurrentTime() - start_time;
    auto probed_bitrate = (media_packets_sent - 1) * kPacketSize / elapsed_time;

    // Validate first cluster bitrate. Not that we have to account for
    // number of intervals and hence |media_packets_sent - 1| on the first cluster.
    EXPECT_NEAR(probed_bitrate.bps(), kFirstClusterBitrate.bps(), kProbingErrorMargin.bps());
    EXPECT_EQ(1u, packet_sender_.padding_sent());

    clock_.AdvanceTime(TimeUntilNextProcess());
    start_time = clock_.CurrentTime();
    while (packet_sender_.media_packets_sent() < 10) {
        ProcessNext();
    }
    // The media packets sent this time.
    media_packets_sent = packet_sender_.media_packets_sent() - media_packets_sent;
    elapsed_time = clock_.CurrentTime() - start_time;
    probed_bitrate = (media_packets_sent - 1) * kPacketSize / elapsed_time;
    EXPECT_NEAR(probed_bitrate.bps(), kSecondClusterBitrate.bps(), kProbingErrorMargin.bps());
}

MY_TEST_F(PacingControllerTest, DISABLED_SkipsProbesWhenProcessIntervalTooLarge) {
    const size_t kPacketSize = 1200;
    const DataRate kInitialBitrate = DataRate::KilobitsPerSec(300);
    const DataRate kProbeBitrate = DataRate::KilobitsPerSec(10'000); // 10Mbps
    const int kProbeClusterId = 3;

    // Test with both legacy and new probe discard modes.
    for (bool abort_delayed_probes : {false, true}) {
        pacing_config_.probing_setting.abort_delayed_probes = abort_delayed_probes;
        pacing_config_.probing_setting.max_probe_delay = TimeDelta::Millis(2);
        
        SetUp();

        pacer_->SetPacingBitrate(kInitialBitrate * PacingController::kDefaultPaceMultiplier, kInitialBitrate);

        uint16_t seq_num = 100;
        for (int i = 0; i < 10; ++i) {
            EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        }

        while (pacer_->NumQueuedPackets() > 0) {
            ProcessNext();
        }

        // Probe at a very high bitrate.
        pacer_->AddProbeCluster(kProbeClusterId, kProbeBitrate);
        // We need one packet to start the probe.
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);

        const size_t packets_sent_before_probing = packet_sender_.media_packets_sent();
        clock_.AdvanceTime(TimeUntilNextProcess());
        pacer_->ProcessPackets();
        // Probing with the non-padding packets in queue first, then send padding packet instead.
        EXPECT_EQ(packets_sent_before_probing + 1, packet_sender_.media_packets_sent());

        auto start_time = clock_.CurrentTime();
        clock_.AdvanceTime(TimeUntilNextProcess());
        auto time_between_probes = clock_.CurrentTime() - start_time;
        // Advance that distance again + 1ms.
        clock_.AdvanceTime(time_between_probes);

        // Send second probe packet.
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
        pacer_->ProcessPackets();
        EXPECT_EQ(packets_sent_before_probing + 2, packet_sender_.media_packets_sent());
        EXPECT_EQ(packet_sender_.last_pacing_info().probe_cluster->id, kProbeClusterId);

        // We're exactly where we should be for the next probe after last process done.
        auto next_probe_time = clock_.CurrentTime();
        EXPECT_EQ(pacer_->NextSendTime(), next_probe_time);

        // Advance to within max probe delay, should still return same next time.
        // |now - next_probe_time == max_probe_delay|
        clock_.AdvanceTime(pacing_config_.probing_setting.max_probe_delay);
        EXPECT_EQ(pacer_->NextSendTime(), next_probe_time);

        // Too delay to probe, drop it. |now - next_probe_time > max_probe_delay|
        clock_.AdvanceTimeUs(1);

        const size_t bytes_sent_before_timeout = packet_sender_.total_bytes_sent();
        if (abort_delayed_probes) {
            // Expected next process time is unchanged, but calling should not
            // generate new packets.
            EXPECT_EQ(pacer_->NextSendTime(), next_probe_time);
            pacer_->ProcessPackets();
            EXPECT_EQ(bytes_sent_before_timeout, packet_sender_.total_bytes_sent());

            // Next packet sent is not part of probe
            ProcessNext();
            EXPECT_FALSE(packet_sender_.last_pacing_info().probe_cluster.has_value());
            
        } else {
            // Legacy behaviour, probe "aborted" so send time moved back. Next call to
            // ProcessPackets() still results in packets being marked as part of probe
            // cluster.
            EXPECT_GT(pacer_->NextSendTime(), next_probe_time);
            size_t padding_sent_before_probe = packet_sender_.padding_sent();
            ProcessNext();
            EXPECT_GT(packet_sender_.total_bytes_sent(), bytes_sent_before_timeout);
            EXPECT_EQ(packet_sender_.last_pacing_info().probe_cluster->id, kProbeClusterId);
            // As no media packets in queue, we will send padding packets instead.
            EXPECT_GT(packet_sender_.padding_sent(), padding_sent_before_probe);

            // Time between sent packets keeps being too large, but we still mark the
            // packets as being part of the cluster.
            auto start_probing_time = clock_.CurrentTime();
            padding_sent_before_probe = packet_sender_.padding_sent();
            ProcessNext();
            EXPECT_GT(packet_sender_.total_bytes_sent(), bytes_sent_before_timeout);
            EXPECT_EQ(packet_sender_.last_pacing_info().probe_cluster->id, kProbeClusterId);
            EXPECT_GT(clock_.CurrentTime() - start_probing_time, time_between_probes);
            EXPECT_GT(packet_sender_.padding_sent(), padding_sent_before_probe);
        }
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_ProbingWithPaddingSupport) {
    const size_t kPacketSize = 1200;
    const DataRate kInitialBitrate = DataRate::KilobitsPerSec(300);

    pacer_->AddProbeCluster(0, kFirstClusterBitrate);
    pacer_->SetPacingBitrate(kInitialBitrate * PacingController::kDefaultPaceMultiplier, DataRate::Zero());

    uint16_t seq_num = 100;
    for (int i = 0; i < 3; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }
    
    const auto start_time = clock_.CurrentTime();
    int process_count = 0;
    while (process_count < 5) {
        ProcessNext();
        ++process_count;
    }
    // The media packet will be sent prior to the padding packets.
    EXPECT_EQ(3U, packet_sender_.media_packets_sent());
    // Will send padding packet instead if no media packet in queue.
    EXPECT_GE(packet_sender_.padding_sent(), 0);
    auto probed_bitrate = packet_sender_.total_bytes_sent() / (clock_.CurrentTime() - start_time);

    EXPECT_NEAR(probed_bitrate.bps(), kFirstClusterBitrate.bps(), kProbingErrorMargin.bps());
}

MY_TEST_F(PacingControllerTest, DISABLED_DontSendPaddingIfQueueIsNonEmpty) {
    const size_t kPacketSize = 1200;
    // Initially no padding bitrate.
    pacer_->SetPacingBitrate(DataRate::BitsPerSec(60'000 * PacingController::kDefaultPaceMultiplier), DataRate::Zero());

    uint16_t seq_num = 100;
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    pacer_->ProcessPackets();

    // Add 30kbps padding. When increase budget, media budget will increase from
    // negative (overuse) while padding budget will increase from 0 (as padding bitrate is zero).
    clock_.AdvanceTimeMs(5);
    // 150000 bps
    pacer_->SetPacingBitrate(DataRate::BitsPerSec(60'000 * PacingController::kDefaultPaceMultiplier), DataRate::BitsPerSec(30'000));

    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_GT(pacer_->ExpectedQueueTime(), TimeDelta::Millis(5));

    // Don't send padding if queue is non-empty, even if padding debut == 0.
    EXPECT_CALL(packet_sender_, SendPadding).Times(0);
    ProcessNext();
}

MY_TEST_F(PacingControllerTest, DISABLED_ProbeClusterId) {
    const size_t kPacketSize = 1200;

    pacer_->SetPacingBitrate(kTargetRate * PacingController::kDefaultPaceMultiplier, kTargetRate);
    pacer_->SetProbingEnabled(true);

    pacer_->AddProbeCluster(0, kFirstClusterBitrate);
    pacer_->AddProbeCluster(1, kSecondClusterBitrate);

    uint16_t seq_num = 100;
    for (int i = 0; i < 10; ++i) {
        EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    }

    // Using media packets for probing.
    // First probing cluster.
    EXPECT_CALL(packet_sender_, SendProbe(RtpPacketType::PADDING, _, /*probe_cluster_id=*/0)).Times(1);
    EXPECT_CALL(packet_sender_, SendProbe(RtpPacketType::VIDEO, _, /*probe_cluster_id=*/0)).Times(5);
    for (int i = 0; i < 5; ++i) {
        ProcessNext();
    }

    // Second probing cluster.
    EXPECT_CALL(packet_sender_, SendProbe(RtpPacketType::PADDING, _, /*probe_cluster_id=*/1)).Times(1);
    EXPECT_CALL(packet_sender_, SendProbe(RtpPacketType::VIDEO, _, /*probe_cluster_id=*/1)).Times(5);
    for (int i = 0; i < 5; ++i) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_OwnedPacketPrioritiedOnTyep) {
    const size_t kPacketSize = 1200;
    uint16_t seq_num = 100;

    // Insert a packet of each type, from low to high priority.
    // Since priority is weighted higher than insert order, 
    // these should come out of the pacer in backwards order 
    // except the FEC and Video packet (they have the same priority).
    for (auto type : {RtpPacketType::PADDING, 
                      RtpPacketType::FEC,
                      RtpPacketType::VIDEO,
                      RtpPacketType::RETRANSMISSION,
                      RtpPacketType::AUDIO}) {
        EnqueuePacket(type, 1234, seq_num++, clock_.now_ms(), kPacketSize);
    }

    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::AUDIO, _, _, _, _));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::RETRANSMISSION, _, _, _, _));
    // FEC and video actually have the same priority, so they will come
    // out in insertion order.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::FEC, _, _, _, _));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, _, _, _, _));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, _, _, _, _));

    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_SmallFirstProbePacket) {
    const size_t kPacketSize = 1200;
    pacer_->AddProbeCluster(0, kFirstClusterBitrate);

    uint16_t seq_num = 100;
    // Add high prio media
    EnqueuePacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);

    // Expect a samll padding packet to be requestd.
    EXPECT_CALL(packet_sender_, SendPadding(1)).WillOnce([&](size_t padding_size){
        return padding_size;
    });

    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_TaskLate) {
    // Set a low send bitrate to more easily test timing issues.
    pacer_->SetPacingBitrate(DataRate::KilobitsPerSec(30), DataRate::Zero());

    const size_t kPacketSize = 1200;
    uint16_t seq_num = 100;
    // Add four packets of equal size and priority.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);

    // Process packets, only first should be sent.
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    pacer_->ProcessPackets();

    auto next_send_time = pacer_->NextSendTime();
    const auto time_between_packets = next_send_time - clock_.CurrentTime();

    // Simulate a late process call, executed just before we allow
    // sending the fourth packet.
    const auto kOffset = TimeDelta::Millis(1);
    clock_.AdvanceTime((time_between_packets * 3) - kOffset);

    // Process the second and third packets.
    EXPECT_CALL(packet_sender_, SendPacket).Times(2);
    pacer_->ProcessPackets();

    // Check next scheduled send time
    next_send_time = pacer_->NextSendTime();
    const auto time_left = next_send_time - clock_.CurrentTime();
    EXPECT_EQ(time_left.RoundTo(TimeDelta::Millis(1)), kOffset);

    // Process the last packet.
    clock_.AdvanceTime(time_left);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    pacer_->ProcessPackets();
}

MY_TEST_F(PacingControllerTest, DISABLED_NoProbingWhilePaused) {
    // Add a larger probing bitrate to cause a small interval.
    pacer_->AddProbeCluster(3, DataRate::KilobitsPerSec(10'000)); // 10Mbps
    pacer_->SetProbingEnabled(true);

    const size_t kPacketSize = 1000;
    uint16_t seq_num = 100;
    // recommended_probe_size = 2 * 10'000'000bps * 1ms / 8 = 2500 bytes
    // padding_to_add = recommended_probe_size - media_sent - 1 bytes smalll padding = 2500 - 1000 - 1 = 1499;
    // padding_packets = (padding_to_add + 223) / 224 = 7
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(7 + 1);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, _)).Times(1);
    // Send at least one packet so probing can initiate.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    while (pacer_->NumQueuedPackets() > 0) {
        ProcessNext();
    }

    // Time to next send time should be small.
    EXPECT_LT(pacer_->NextSendTime() - clock_.CurrentTime(), PacingController::kPausedProcessInterval);

    // Pause pacer, time to next send time should be the pause process interval now.
    pacer_->Pause();

    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), PacingController::kPausedProcessInterval);
}

MY_TEST_F(PacingControllerTest, DISABLED_AudioNotPacedEvenWhenAccountedFor) {
    // Account for audio - so that audio packets can cause pushback on other
    // types such as video. Audio packet should still be immediated passed
    // through though ("WebRTC-Pacer-BlockAudio" needs to be enabled in order
    // to pace audio packets).
    pacer_->set_account_for_audio(true);

    const size_t kPacketSize = 123;
    uint16_t seq_num = 100;

    // Set pacing bitrate 1 packet per second, no padding.
    pacer_->SetPacingBitrate(kPacketSize / TimeDelta::Seconds(1), DataRate::Zero());

    // Add and send an audio packet.
    EnqueueAndVerifyPacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    pacer_->ProcessPackets();

    // Advance time but not reach the next send time, add another audio packet and process.
    // It should be sent immediately.
    clock_.AdvanceTimeMs(5); // 5ms < 1s
    EnqueueAndVerifyPacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    pacer_->ProcessPackets();
}

MY_TEST_F(PacingControllerTest, DISABLED_PaddingResumesAfterSaturationEvenWithConcurrentAudio) {
    const DataRate kPacingBitrate = DataRate::KilobitsPerSec(125);  // 125 kbps
    const DataRate kPaddingBitrate = DataRate::KilobitsPerSec(100); // 100 kbps
    const TimeDelta kMaxBufferInTime = TimeDelta::Millis(500);
    const size_t kPacketSize = 130;
    const TimeDelta kAudioPacketInterval = TimeDelta::Millis(20);

    // In this test, we first send a burst of video in order to saturate the
    // padding debt level.
    // We then proceed to send audio at a bitrate that is slightly lower than
    // the padding rate, meaning there will be a period with audio but no
    // padding sent while the debt is draining, then audio and padding will
    // be interlieved.
    for (bool account_for_audio : {false, true}) {
        uint16_t seq_num = 100;
        pacer_->set_account_for_audio(account_for_audio);

        // First, saturate the padding padding debt level.
        pacer_->SetPacingBitrate(kPacingBitrate, kPaddingBitrate);

        const auto kPaddingSaturationTime = kMaxBufferInTime * kPaddingBitrate / (kPacingBitrate - kPaddingBitrate);
        const size_t video_to_send = kPaddingSaturationTime * kPacingBitrate;
        const size_t kVideoPacketSize = 1200;
        size_t video_sent = 0;
        // Enqueue video packets to saturate the padding debt level.
        while (video_sent < video_to_send) {
            EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kVideoPacketSize);
            video_sent += kVideoPacketSize;
        }
        // Pay off the media debt but the padding debt is still saturated.
        while (pacer_->NumQueuedPackets() > 0) {
            ProcessNext();
        }

        // Add a stream of audio packets at a rate slightly lower than the padding
        // rate, once the padding debt is paid off we expect padding to be
        // generated.
        bool padding_seen = false;
        EXPECT_CALL(packet_sender_, SendPadding).WillOnce([&](size_t padding_size){
            padding_seen = true;
            return padding_size;
        });

        auto start_time = clock_.CurrentTime();
        auto last_audio_time = start_time;
        while (!padding_seen) {
            auto now = clock_.CurrentTime();
            auto next_send_time = pacer_->NextSendTime();
            auto wait_time = std::min(next_send_time, last_audio_time + kAudioPacketInterval) - now;
            // Advance time to send next audio.
            clock_.AdvanceTime(wait_time);
            // Enque audio packet at intervals.
            while (clock_.CurrentTime() >= last_audio_time + kAudioPacketInterval) {
                EnqueuePacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);
                last_audio_time += kAudioPacketInterval;
            }
            pacer_->ProcessPackets();
        }

        // Verify how long it took to drain the padding debt.
        // Allow 2% error margin.
        const DataRate kAudioBitrate = kPacketSize / kAudioPacketInterval;
        const TimeDelta expected_drain_time = account_for_audio ? ((kMaxBufferInTime * kPaddingBitrate) / (kPaddingBitrate - kAudioBitrate))
                                                                : kMaxBufferInTime;
        const TimeDelta actual_drain_time = clock_.CurrentTime() - start_time;
        GTEST_COUT << "padding_bitrate=" << kPaddingBitrate.kbps() 
                   << " kbps - audio_bitrate=" << kAudioBitrate.kbps() 
                   << " kbps - expected_drain_time=" << expected_drain_time.ms()
                   << " ms - actual_drain_time=" << actual_drain_time.ms()
                   << " ms." << std::endl;
        EXPECT_NEAR(actual_drain_time.ms(), expected_drain_time.ms(), expected_drain_time.ms() * 0.02) 
            << " where account_for_audio = "
            << (account_for_audio ? "true" : "false");
    }
}

MY_TEST_F(PacingControllerTest, DISABLED_AccountsForAudioEnqueueTime) {
    const DataRate kPacingBitrate = DataRate::KilobitsPerSec(125);
    const size_t kPacketSize = 130;
    const TimeDelta kPacketPacingTime = kPacketSize / kPacingBitrate;

    // Audio not paced, but still accounted for in budget.
    pacer_->set_account_for_audio(true);
    pacer_->SetPacingBitrate(kPacingBitrate, DataRate::Zero());

    // Enqueue two audio packets, advance time to where ont packet
    // should be drained the buffer already, has they been sent
    // immediately.
    uint16_t seq_num = 100;
    EnqueueAndVerifyPacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EnqueueAndVerifyPacket(RtpPacketType::AUDIO, kAudioSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    clock_.AdvanceTime(kPacketPacingTime);
    // The time to send unpaced audio packets are their enqueue time, 
    // so both packets were sent.
    pacer_->ProcessPackets();

    // Add a video packet. we can't be sent untill debt from
    // audio packet has been drained.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), kPacketPacingTime);
    
}

MY_TEST_F(PacingControllerTest, DISABLED_NextSendTimeAccountsForPadding) {
    const DataRate kPacingBitrate = DataRate::KilobitsPerSec(125);
    const size_t kPacketSize = 130;
    const TimeDelta kPacketPacingTime = kPacketSize / kPacingBitrate;

    // Start with no padding.
    pacer_->SetPacingBitrate(kPacingBitrate, DataRate::Zero());
    
    uint16_t seq_num = 100;
    // Send a single packet.
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    pacer_->ProcessPackets();

    // With current conditions, no need to wake until next keep-alive.
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), PacingController::kPausedProcessInterval);

    // Enqueue a new packet, but can't be sent until previous buffer has
    // drained.
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), kPacketPacingTime);
    // Advance time to drain the media debt.
    clock_.AdvanceTime(kPacketPacingTime);
    pacer_->ProcessPackets();

    // With current condistions, again no need to wake until next keep-alive.
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), PacingController::kPausedProcessInterval);

    // Set a non-zero padding bitrate. Padding also can't be sent until
    // previous debt has cleared. Since Padding was disabled before,
    // there currently is no padding debt.
    pacer_->SetPacingBitrate(kPacingBitrate, kPacingBitrate / 2);
    // Time to drain the media debt.
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), kPacketPacingTime);

    // Advance time, and send padding with |kPacketSize| bytes.
    EXPECT_CALL(packet_sender_, SendPadding).WillOnce(Return(kPacketSize));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    clock_.AdvanceTime(kPacketPacingTime);
    pacer_->ProcessPackets();

    // Since padding rate is half of pacing rate, next time we can send
    // padding is double the packet pacing time.
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(),
              kPacketPacingTime * 2);
    
    // Insert a packet to be sent, this take precedence again.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_EQ(pacer_->NextSendTime() - clock_.CurrentTime(), kPacketPacingTime);
}

MY_TEST_F(PacingControllerTest, DISABLED_PaddingTargetAccountsForPaddingRate) {
    const DataRate kPacingBitrate = DataRate::KilobitsPerSec(125);
    const size_t kPacketSize = 130;

    // Reset pacer with explicitly set padding target of 10ms.
    const TimeDelta kPaddingTarget = TimeDelta::Millis(10);
    pacing_config_.pacing_setting.padding_target_duration = kPaddingTarget; // 10ms
    SetUp();

    // Start with pacing and padding bitrate equal.
    pacer_->SetPacingBitrate(kPacingBitrate, kPacingBitrate);

    uint16_t seq_num = 100;
    // Send a video packet.
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    ProcessNext();
    
    // Send padding instead if no media packet in queue.
    size_t expected_padding_target_bytes = (kPaddingTarget * kPacingBitrate);
    EXPECT_CALL(packet_sender_, SendPadding(expected_padding_target_bytes)).WillOnce(Return(expected_padding_target_bytes));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    ProcessNext();

    // Half the padding bitrate, and expect half the padding target.
    pacer_->SetPacingBitrate(kPacingBitrate, kPacingBitrate / 2);
    expected_padding_target_bytes = expected_padding_target_bytes / 2;
    EXPECT_CALL(packet_sender_, SendPadding(expected_padding_target_bytes)).WillOnce(Return(expected_padding_target_bytes));
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::PADDING, kPaddingSsrc, _, _, _)).Times(1);
    ProcessNext();
}

MY_TEST_F(PacingControllerTest, DISABLED_SendsFECPackets) {
    const size_t kPacketSize = 123;

    // Set pacing bitrate to 1000 packets per second, and no padding.
    const DataRate kPacingBitrate = kPacketSize * 1000 / TimeDelta::Seconds(1);
    pacer_->SetPacingBitrate(kPacingBitrate, DataRate::Zero());

    uint16_t seq_num = 100;
    uint16_t fec_seq_num = 999;
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, _, _, _)).Times(1);
    EXPECT_CALL(packet_sender_, FetchFecPackets).WillOnce([&](){
        EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::FEC, kFlexFecSsrc, _, _, _)).Times(1);
        // Don't provide FEC protection for FEC packets.
        EXPECT_CALL(packet_sender_, FetchFecPackets);
        std::vector<RtpPacketToSend> fec_packets;
        fec_packets.push_back(BuildPacket(RtpPacketType::FEC, kFlexFecSsrc, fec_seq_num++, clock_.now_ms(), kPacketSize));
        return fec_packets;
    });
    // Process non-fec packets
    ProcessNext();
    // Process FEC packets
    ProcessNext();
}

MY_TEST_F(PacingControllerTest, DISABLED_GapInPacingDoesntAccoumulateBudget) {
    const size_t kPacketSize = 250;
    const TimeDelta kPacketSendTime = TimeDelta::Millis(15);

    pacer_->SetPacingBitrate(kPacketSize / kPacketSendTime, DataRate::Zero());

    uint16_t seq_num = 100;
    // Send an initial packet.
    EnqueueAndVerifyPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num++, clock_.now_ms(), kPacketSize);
    pacer_->ProcessPackets();

    // Advance time |kPacketSendTime| past where the media debt should be 0.
    clock_.AdvanceTime(kPacketSendTime);

    // Enqueue two new packets, and expect only one to be sent after one process called.
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num + 1, clock_.now_ms(), kPacketSize);
    EnqueuePacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num + 2, clock_.now_ms(), kPacketSize);
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc, seq_num + 1, _, kPacketSize));
    pacer_->ProcessPackets();
}

} // namespace test    
} // namespace naivertc
