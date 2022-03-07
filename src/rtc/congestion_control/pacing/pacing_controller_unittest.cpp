#include "rtc/congestion_control/pacing/pacing_controller.hpp"

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
// Process 5 times per second.
constexpr TimeDelta kProcessInterval = TimeDelta::Millis(200);

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 23456;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(800);

RtpPacketToSend BuildPacket(RtpPacketType type,
                            uint32_t ssrc,
                            uint16_t seq_num,
                            int64_t capture_time_ms,
                            size_t payload_size) {
    auto packet = RtpPacketToSend(nullptr);
    packet.set_packet_type(type),
    packet.set_ssrc(ssrc),
    packet.set_capture_time_ms(capture_time_ms),
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
        SendPacket(packet.ssrc(),
                   packet.packet_type(),
                   packet.sequence_number(),
                   packet.capture_time_ms(),
                   packet.payload_size());
    }

    std::vector<RtpPacketToSend> GeneratePadding(size_t padding_size) override {
        std::vector<RtpPacketToSend> packets;
        padding_size = SendPadding(padding_size);
        if (padding_size > 0) {
            RtpPacketToSend packet(nullptr);
            packet.set_payload_size(padding_size);
            packet.set_packet_type(RtpPacketType::PADDING);
            packets.emplace_back(std::move(packet));
        }
        return packets;
    }

    MOCK_METHOD(void, 
                SendPacket, 
                (uint32_t ssrc,
                RtpPacketType packet_type,
                uint16_t seq_num, 
                int64_t capture_time_ms, 
                size_t payload_size));

    MOCK_METHOD(std::vector<RtpPacketToSend>,
                FetchFecPackets,
                (),
                (override));
    MOCK_METHOD(size_t, SendPadding, (size_t target_size));
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
    }

    void EnqueuePacketFrom(MediaStream& stream) {
        pacer_->EnqueuePacket(BuildPacket(stream.packet_type, 
                                          stream.ssrc, 
                                          stream.seq_num++, 
                                          clock_.now_ms(), 
                                          stream.packet_size));
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

MY_TEST_F(PacingControllerTest, DefaultNoPaddingInSilence) {
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

MY_TEST_F(PacingControllerTest, EnablePaddingInSilence) {
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

MY_TEST_F(PacingControllerTest, EnablePacingAudio) {
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
    EXPECT_CALL(packet_sender_, SendPacket(kAudioSsrc, RtpPacketType::AUDIO, _, _, _)).Times(0);
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

MY_TEST_F(PacingControllerTest, DefaultNotPacingAudio) {
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

MY_TEST_F(PacingControllerTest, DefaultBudetNotAffectAudio) {
    // 1000 / 3 * 8 * 0.2 = 1600 / 3 = 533 kbs.
    auto pacing_bitrate = DataRate::BitsPerSec(kVideoStream.packet_size / 3 * 8 * kProcessInterval.ms());
    pacer_->SetPacingBitrate(pacing_bitrate, DataRate::Zero());

    // Video fills budget for following process periods.
    EnqueuePacketFrom(kVideoStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();

    // Audio not blocked due to budget limit.
    EnqueuePacketFrom(kAudioStream);
    EXPECT_CALL(packet_sender_, SendPacket).Times(1);
    ProcessNext();

}
    
} // namespace test    
} // namespace naivertc
