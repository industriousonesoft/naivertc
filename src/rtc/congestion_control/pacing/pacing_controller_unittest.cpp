#include "rtc/congestion_control/pacing/pacing_controller.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr DataRate kFirstClusterBitrate = DataRate::KilobitsPerSec(900);
constexpr DataRate kSecondClusterBitrate = DataRate::KilobitsPerSec(1800);

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
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

} // namespace

class MockPacingPacketSender : public PacingController::PacketSender {
public:
    void SendPacket(RtpPacketToSend packet, 
                    const PacedPacketInfo& pacing_info) override {
        SendPacket(packet.ssrc(),
                   packet.sequence_number(),
                   packet.capture_time_ms(),
                   packet.payload_size());
    }

    std::vector<RtpPacketToSend> GeneratePadding(size_t padding_size) override {
        std::vector<RtpPacketToSend> packets;
        SendPadding(padding_size);
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
                uint16_t seq_num, 
                int64_t capture_time_ms, 
                size_t payload_size));

    MOCK_METHOD(std::vector<RtpPacketToSend>,
                FetchFecPackets,
                (),
                (override));
    MOCK_METHOD(size_t, SendPadding, (size_t target_size));
};

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


    
} // namespace test    
} // namespace naivertc
