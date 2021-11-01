#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace naivertc;

namespace naivertc {
// namespace test {
namespace {

constexpr size_t kFecPayloadType = 96;
constexpr size_t kRedPayloadType = 97;
constexpr size_t kVideoPayloadType = 98;
constexpr uint32_t kMediaSsrc = 835424;

} // namespace

void VerifyRtpHeader(uint16_t seq_num,
                     uint32_t timestamp,
                     size_t red_payload_type,
                     size_t fec_payload_type,
                     bool marker,
                     size_t payload_offset,
                     const uint8_t* data) {
    EXPECT_EQ(marker ? 0x80 : 0x00, data[1] & 0x80);
    EXPECT_EQ(red_payload_type, data[1] & 0x7F);
    EXPECT_EQ(seq_num, (data[2] << 8) | data[3]);
    EXPECT_EQ(timestamp, (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]);
    EXPECT_EQ(fec_payload_type, data[payload_offset]);
}

class FEC_UlpFecGeneratorTest : public ::testing::Test, public naivertc::UlpFecGenerator {
public:
    FEC_UlpFecGeneratorTest() 
        : UlpFecGenerator(kRedPayloadType, kFecPayloadType) {}
};

TEST_F(FEC_UlpFecGeneratorTest, NoEmptyFecWithSeqNumGaps) {
    struct Packet {
        size_t header_size;
        size_t payload_size;
        uint16_t seq_num;
        bool marker;
    };
    std::vector<Packet> protected_packets;
    protected_packets.push_back({15, 3, 41, 0});
    protected_packets.push_back({14, 1, 43, 0});
    protected_packets.push_back({19, 0, 48, 0});
    protected_packets.push_back({19, 0, 50, 0});
    protected_packets.push_back({14, 3, 51, 0});
    protected_packets.push_back({13, 8, 52, 0});
    protected_packets.push_back({19, 2, 53, 0});
    protected_packets.push_back({12, 3, 54, 0});
    protected_packets.push_back({21, 0, 55, 0});
    protected_packets.push_back({13, 3, 57, 1});
    FecProtectionParams params = {117, 3, FecMaskType::BURSTY};
    SetProtectionParameters(params, params);
    for (auto& p : protected_packets) {
        auto rtp_packet = std::make_shared<RtpPacketToSend>(nullptr);
        rtp_packet->set_marker(p.marker);
        rtp_packet->set_sequence_number(p.seq_num);
        rtp_packet->AllocatePayload(p.payload_size);
        PushMediaPacket(std::move(rtp_packet));

        auto fec_packets = PopFecPackets();
        // The packet is not the last one in frame
        if (!p.marker) {
            EXPECT_TRUE(fec_packets.empty());
        } else {
            EXPECT_FALSE(fec_packets.empty());
        }
    }
}

TEST_F(FEC_UlpFecGeneratorTest, OneFrameFec) {
    const size_t kNumMediaPackets = 4;
    FecProtectionParams params = {15, 3, FecMaskType::RANDOM};
    SetProtectionParameters(params, params);
    uint32_t last_timestamp = 0;
    for (size_t i = 0; i < kNumMediaPackets; ++i) {
        auto media_packet = std::make_shared<RtpPacketToSend>(nullptr);
        media_packet->set_sequence_number(i + 100);
        media_packet->set_timestamp(1000 + i);
        media_packet->set_payload_type(kVideoPayloadType);
        media_packet->SetPayloadSize(100);
        media_packet->set_marker(i == kNumMediaPackets - 1);
        last_timestamp = media_packet->timestamp();
        PushMediaPacket(std::move(media_packet));
    }

    auto fec_packets = PopFecPackets();
    EXPECT_EQ(fec_packets.size(), 1u);
    uint16_t seq_num = kNumMediaPackets + 100;
    fec_packets[0]->set_sequence_number(seq_num);
    EXPECT_TRUE(PopFecPackets().empty());

    EXPECT_EQ(fec_packets[0]->header_size(), kRtpHeaderSize);

    VerifyRtpHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType, false, kRtpHeaderSize, fec_packets[0]->data());
}

TEST_F(FEC_UlpFecGeneratorTest, TwoFrameFec) {
    const size_t kNumMediaFrames = 2;
    const size_t kNumMediaPackets = 2;
    FecProtectionParams params = {15, 3, FecMaskType::RANDOM};
    SetProtectionParameters(params, params);
    uint16_t seq_num = 100;
    uint32_t last_timestamp = 0;
    for (size_t frame_i = 0; frame_i < kNumMediaFrames; ++frame_i) {
        for (size_t i = 0; i < kNumMediaPackets; ++i) {
            auto media_packet = std::make_shared<RtpPacketToSend>(nullptr);
            media_packet->set_sequence_number(seq_num++);
            media_packet->set_timestamp(1000 + i);
            media_packet->set_payload_type(kVideoPayloadType);
            media_packet->SetPayloadSize(100);
            media_packet->set_marker(i == kNumMediaPackets - 1);
            last_timestamp = media_packet->timestamp();
            PushMediaPacket(std::move(media_packet));
        }
    }
    
    auto fec_packets = PopFecPackets();
    EXPECT_EQ(fec_packets.size(), 1u);
    fec_packets[0]->set_sequence_number(seq_num);
    EXPECT_TRUE(PopFecPackets().empty());

    EXPECT_EQ(fec_packets[0]->header_size(), kRtpHeaderSize);

    VerifyRtpHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType, false, kRtpHeaderSize, fec_packets[0]->data());
}

TEST_F(FEC_UlpFecGeneratorTest, UpdateProtectionParameters) {
    const FecProtectionParams kKeyFrameParams = {25, 2 /*max_fec_frames*/, FecMaskType::RANDOM};
    const FecProtectionParams kDeltaFrameParams = {25, 5/*max_fec_frames*/, FecMaskType::RANDOM};

    SetProtectionParameters(kDeltaFrameParams, kKeyFrameParams);

    EXPECT_EQ(CurrentParams().max_fec_frames, 0);
    uint16_t seq_num = 100;
    auto add_frame = [&](bool is_key_frame, uint16_t seq_num) {
        auto media_packet = std::make_shared<RtpPacketToSend>(nullptr);
        media_packet->set_sequence_number(seq_num++);
        media_packet->set_timestamp(seq_num);
        media_packet->set_payload_type(kVideoPayloadType);
        media_packet->SetPayloadSize(10);
        media_packet->set_is_key_frame(is_key_frame);
        media_packet->set_marker(true);
        PushMediaPacket(std::move(media_packet));
    };

    // Add key-frame, keyframe params should apply, no FEC generated yet.
    add_frame(true, seq_num++);
    EXPECT_EQ(CurrentParams().max_fec_frames, 2);
    EXPECT_TRUE(PopFecPackets().empty());

    // Add delta-frame, generated FEC packet. Params will not be updated until
    // next added packet though.
    add_frame(false, seq_num++);
    EXPECT_EQ(CurrentParams().max_fec_frames, 2);
    EXPECT_FALSE(PopFecPackets().empty());

    // Add delta-frame, now params get updated.
    add_frame(false, seq_num++);
    EXPECT_EQ(CurrentParams().max_fec_frames, 5);
    EXPECT_TRUE(PopFecPackets().empty());

    // Add yet another delta-frame.
    add_frame(false, seq_num++);
    EXPECT_EQ(CurrentParams().max_fec_frames, 5);
    EXPECT_TRUE(PopFecPackets().empty());

    // Add key-frame, params immediately switch to key-frame ones. The two
    // buffered frames plus the key-frame is protected and fec emitted,
    // even though the frame count is technically over the keyframe frame count
    // threshold.
    add_frame(true, seq_num++);
    EXPECT_EQ(CurrentParams().max_fec_frames, 2);
    EXPECT_FALSE(PopFecPackets().empty());
}
    
// } // namespace test
} // namespace naivertc 