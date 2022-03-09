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
        total_bytes_sent_ += packet.payload_size();
        SendPacket(packet.packet_type(),
                   packet.ssrc(),
                   packet.sequence_number(),
                   packet.capture_time_ms(),
                   packet.payload_size());
    }

    std::vector<RtpPacketToSend> GeneratePadding(size_t padding_size) override {
        std::vector<RtpPacketToSend> packets;
        padding_size = SendPadding(padding_size);
        if (padding_size > 0) {
            RtpPacketToSend packet(nullptr);
            packet.set_ssrc(kPaddingSsrc);
            packet.set_payload_size(padding_size);
            packet.set_packet_type(RtpPacketType::PADDING);
            packets.emplace_back(std::move(packet));
            padding_sent_ += padding_size;
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

    size_t padding_sent() const { return padding_sent_; }
    size_t total_bytes_sent() const { return total_bytes_sent_; }

private:
    size_t padding_sent_;
    size_t total_bytes_sent_;
};

// MockPacingPacketSenderPadding
class MockPacingPacketSenderPadding : public PacingController::PacketSender {
public:
    // From RTPSender:
    // Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP
    static const size_t kPaddingPacketSize = 224;

    MockPacingPacketSenderPadding() : padding_sent_(0), total_bytes_sent_(0) {}

    void SendPacket(RtpPacketToSend packet, const PacedPacketInfo& pacing_info) override {
        total_bytes_sent_ += packet.payload_size();
    }

    std::vector<RtpPacketToSend> FetchFecPackets() override {
        return {};
    }

    std::vector<RtpPacketToSend> GeneratePadding(size_t padding_size) override {
        size_t num_packets = (padding_size + kPaddingPacketSize - 1) / kPaddingPacketSize;
        std::vector<RtpPacketToSend> packets;
        for (int i = 0; i < num_packets; ++i) {
            RtpPacketToSend packet(nullptr);
            packet.SetPadding(kPaddingPacketSize);
            packet.set_packet_type(RtpPacketType::PADDING);
            packets.emplace_back(std::move(packet));
            padding_sent_ += kPaddingPacketSize;
        }
        return packets;
    }

    size_t padding_sent() const { return padding_sent_; }
    size_t total_bytes_sent() const { return total_bytes_sent_; }

private:
    size_t padding_sent_;
    size_t total_bytes_sent_;
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

    void ProcessNext() {
        auto now = clock_.CurrentTime();
        auto next_send_time = pacer_->NextSendTime();
        auto wait_time = std::max(TimeDelta::Zero(), next_send_time - now);
        clock_.AdvanceTime(wait_time);
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
    // as the media debt can't be payed off by one process。
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
    // as the media debt can't be payed off by one process。
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

MY_TEST_F(PacingControllerTest, DISABLED_Padding) {
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

    EXPECT_CALL(packet_sender_, SendPadding).Times(kPaddingPacketsToSend)
                                            .WillRepeatedly([&](size_t target_size){
        ++padding_packets_sent;
        if (padding_packets_sent < kPaddingPacketsToSend) {
            // Don't count bytes of last packet, instead just
            // use this as the time the last packet finished
            // sending.
            padding_sent += target_size;
        }
        if (first_send_time.IsInfinite()) {
            first_send_time = clock_.CurrentTime();
        } else {
            last_send_time = clock_.CurrentTime();
        }
        return target_size;
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

MY_TEST_F(PacingControllerTest, Priority) {
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

MY_TEST_F(PacingControllerTest, HighPriorityDoesntAffectDebt) {
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

MY_TEST_F(PacingControllerTest, SendsHeartbeatOnlyWhenCongested) {
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
    
// 
} // namespace test    
} // namespace naivertc
