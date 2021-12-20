#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer_h264.hpp"
#include "rtc/base/memory/bit_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"
#include "common/array_view.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

enum Nalu {
    kSlice = 1,
    kIdr = 5,
    kSei = 6,
    kSps = 7,
    kPps = 8,
    kStapA = 24,
    kFuA = 28
};

// Bit masks for FU (A and B) indicators.
enum NalDefs { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

MY_TEST(RtpH264DepacketizerTest, SingleNalu) {
    uint8_t packet[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5 (IDR).
    CopyOnWriteBuffer rtp_payload(packet);

    RtpH264Depacketizer depacketizer;
    std::optional<RtpH264Depacketizer::Packet> parsed = depacketizer.Depacketize(rtp_payload);
    ASSERT_TRUE(parsed.has_value());

    EXPECT_EQ(parsed->video_payload, rtp_payload);
    EXPECT_EQ(parsed->video_header.frame_type, video::FrameType::KEY);
    EXPECT_EQ(parsed->video_header.codec_type, Video::CodecType::H264);
    EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
    const h264::PacketizationInfo& h264 = std::get<h264::PacketizationInfo>(parsed->video_codec_header);
    EXPECT_EQ(h264.packetization_type, h264::PacketizationType::SIGNLE);
    EXPECT_EQ(h264.packet_nalu_type, h264::NaluType::IDR);
    EXPECT_TRUE(h264.has_idr);
    EXPECT_FALSE(h264.has_pps);
    EXPECT_FALSE(h264.has_sps);
}

MY_TEST(RtpH264DepacketizerTest, SingleNaluSpsWithResolution) {
    uint8_t packet[] = {kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50,
                        0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                        0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25};
    CopyOnWriteBuffer rtp_payload(packet);

    RtpH264Depacketizer depacketizer;
    std::optional<RtpH264Depacketizer::Packet> parsed = depacketizer.Depacketize(rtp_payload);
    ASSERT_TRUE(parsed);

    EXPECT_EQ(parsed->video_payload, rtp_payload);
    EXPECT_EQ(parsed->video_header.frame_type, video::FrameType::KEY);
    EXPECT_EQ(parsed->video_header.codec_type, Video::CodecType::H264);
    EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
    EXPECT_EQ(parsed->video_header.frame_width, 1280u);
    EXPECT_EQ(parsed->video_header.frame_height, 720u);
    const auto& h264 = std::get<h264::PacketizationInfo>(parsed->video_codec_header);
    EXPECT_EQ(h264.packetization_type, h264::PacketizationType::SIGNLE);
    EXPECT_TRUE(h264.has_sps);
    EXPECT_FALSE(h264.has_pps);
    EXPECT_FALSE(h264.has_idr);
}

MY_TEST(RtpH264DepacketizerTest, StapAKey) {
    // clang-format off
    const h264::NaluInfo kExpectedNalus[] = { {h264::NaluType::SPS, 0, -1},
                                              {h264::NaluType::PPS, 1, 2},
                                              {h264::NaluType::IDR, -1, 0} };
    uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                        // Length, nal header, payload.
                        0, 0x18, kExpectedNalus[0].type,
                            0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50, 0x05, 0xBA,
                            0x10, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x00, 0x00, 0x03,
                            0x2A, 0xE0, 0xF1, 0x83, 0x25,
                        0, 0xD, kExpectedNalus[1].type,
                            0x69, 0xFC, 0x0, 0x0, 0x3, 0x0, 0x7, 0xFF, 0xFF, 0xFF,
                            0xF6, 0x40,
                        0, 0xB, kExpectedNalus[2].type,
                            0x85, 0xB8, 0x0, 0x4, 0x0, 0x0, 0x13, 0x93, 0x12, 0x0};
    // clang-format on
    CopyOnWriteBuffer rtp_payload(packet);

    RtpH264Depacketizer depacketizer;
    std::optional<RtpH264Depacketizer::Packet> parsed = depacketizer.Depacketize(rtp_payload);
    ASSERT_TRUE(parsed);

    EXPECT_EQ(parsed->video_payload, rtp_payload);
    EXPECT_EQ(parsed->video_header.frame_type, video::FrameType::KEY);
    EXPECT_EQ(parsed->video_header.codec_type, Video::CodecType::H264);
    EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
    const auto& h264 = std::get<h264::PacketizationInfo>(parsed->video_codec_header);
    EXPECT_EQ(h264.packetization_type, h264::PacketizationType::STAP_A);
    // NALU type for aggregated packets is the type of the first packet only.
    EXPECT_EQ(h264.packet_nalu_type, h264::NaluType::SPS);
    ASSERT_EQ(h264.nalus.size(), 3u);
    EXPECT_TRUE(h264.has_sps);
    EXPECT_TRUE(h264.has_pps);
    EXPECT_TRUE(h264.has_idr);

    for (size_t i = 0; i < h264.nalus.size(); ++i) {
        EXPECT_EQ(h264.nalus[i].type, kExpectedNalus[i].type)
            << "Failed parsing nalu type " << i;
        EXPECT_EQ(h264.nalus[i].sps_id, kExpectedNalus[i].sps_id)
            << "Failed parsing nalu sps id " << i;
        EXPECT_EQ(h264.nalus[i].pps_id, kExpectedNalus[i].pps_id)
            << "Failed parsing nalu pps id" << i;
    }
}

MY_TEST(RtpH264DepacketizerTest, ParsePpsIdFromSlice) {
    uint8_t packet[] = {0x85, 0xB8};
    // 1 000010110 1 1 1 000
    BitReader slice_reader(packet, 2);
    uint32_t golomb_tmp;
    // first_mb_in_slice: ue(v)
    if (slice_reader.ReadExpGolomb(golomb_tmp)) {
        EXPECT_EQ(golomb_tmp, 0u);
    }
    // slice_type: ue(v)
    if (slice_reader.ReadExpGolomb(golomb_tmp)) {
        EXPECT_EQ(golomb_tmp, 21u);
    }
    // pic_parameter_set_id: ue(v)
    uint32_t slice_pps_id;
    if (slice_reader.ReadExpGolomb(slice_pps_id)) {
        EXPECT_EQ(slice_pps_id, 0u);
    }
}

MY_TEST(RtpH264DepacketizerTest, StapANaluSpsWithResolution) {
    uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                                // Length (2 bytes), nal header, payload.
                        0x00, 0x19, kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40,
                        0x50, 0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                        0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25, 0x80,
                        0x00, 0x03, kIdr, 0xFF, 0x00, 0x00, 0x04, kIdr, 0xFF,
                        0x00, 0x11};
    CopyOnWriteBuffer rtp_payload(packet);

    RtpH264Depacketizer depacketizer;
    std::optional<RtpH264Depacketizer::Packet> parsed = depacketizer.Depacketize(rtp_payload);
    ASSERT_TRUE(parsed);

    EXPECT_EQ(parsed->video_payload, rtp_payload);
    EXPECT_EQ(parsed->video_header.frame_type, video::FrameType::KEY);
    EXPECT_EQ(parsed->video_header.codec_type, Video::CodecType::H264);
    EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
    EXPECT_EQ(parsed->video_header.frame_width, 1280u);
    EXPECT_EQ(parsed->video_header.frame_height, 720u);
    const auto& h264 = std::get<h264::PacketizationInfo>(parsed->video_codec_header);
    EXPECT_EQ(h264.packetization_type, h264::PacketizationType::STAP_A);
    EXPECT_TRUE(h264.has_sps);
    EXPECT_FALSE(h264.has_pps);
    EXPECT_TRUE(h264.has_idr);
}

MY_TEST(RtpH264DepacketizerTest, EmptyStapARejected) {
    uint8_t lone_empty_packet[] = {kStapA, 0x00, 0x00};
    uint8_t leading_empty_packet[] = {kStapA, 0x00, 0x00, 0x00, 0x04,
                                        kIdr, 0xFF, 0x00, 0x11};
    uint8_t middle_empty_packet[] = {kStapA, 0x00, 0x03, kIdr, 0xFF, 0x00, 0x00,
                                    0x00, 0x00, 0x04, kIdr, 0xFF, 0x00, 0x11};
    uint8_t trailing_empty_packet[] = {kStapA, 0x00, 0x03, kIdr,
                                        0xFF, 0x00, 0x00, 0x00};

    RtpH264Depacketizer depacketizer;
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(lone_empty_packet)));
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(leading_empty_packet)));
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(middle_empty_packet)));
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(trailing_empty_packet)));
}

MY_TEST(RtpH264DepacketizerTest, StapADelta) {
    uint8_t packet[16] = {kStapA,  // F=0, NRI=0, Type=24.
                                    // Length, nal header, payload.
                            0, 0x02, kSlice, 0xFF, 0, 0x03, kSlice, 0xFF, 0x00, 0,
                            0x04, kSlice, 0xFF, 0x00, 0x11};
    CopyOnWriteBuffer rtp_payload(packet);

    RtpH264Depacketizer depacketizer;
    std::optional<RtpH264Depacketizer::Packet> parsed = depacketizer.Depacketize(rtp_payload);
    ASSERT_TRUE(parsed);

    EXPECT_EQ(parsed->video_payload.size(), rtp_payload.size());
    EXPECT_EQ(parsed->video_payload.cdata(), rtp_payload.cdata());

    EXPECT_EQ(parsed->video_header.frame_type, video::FrameType::DELTA);
    EXPECT_EQ(parsed->video_header.codec_type, Video::CodecType::H264);
    EXPECT_TRUE(parsed->video_header.is_first_packet_in_frame);
    const h264::PacketizationInfo& h264 = std::get<h264::PacketizationInfo>(parsed->video_codec_header);
    EXPECT_EQ(h264.packetization_type, h264::PacketizationType::STAP_A);
    // NALU type for aggregated packets is the type of the first packet only.
    EXPECT_EQ(h264.packet_nalu_type, kSlice);
}

MY_TEST(RtpH264DepacketizerTest, FuA) {
    // clang-format off
    uint8_t packet1[] = {
        kFuA,          // F=0, NRI=0, Type=28.
        kSBit | kIdr,  // FU header.
        0x85, 0xB8, 0x0, 0x4, 0x0, 0x0, 0x13, 0x93, 0x12, 0x0  // Payload.
    };
    // clang-format on
    const uint8_t kExpected1[] = {kIdr, 0x85, 0xB8, 0x0,  0x4, 0x0,
                                    0x0,  0x13, 0x93, 0x12, 0x0};

    uint8_t packet2[] = {
        kFuA,  // F=0, NRI=0, Type=28.
        kIdr,  // FU header.
        0x02   // Payload.
    };
    const uint8_t kExpected2[] = {0x02};

    uint8_t packet3[] = {
        kFuA,          // F=0, NRI=0, Type=28.
        kEBit | kIdr,  // FU header.
        0x03           // Payload.
    };
    const uint8_t kExpected3[] = {0x03};

    RtpH264Depacketizer depacketizer;
    std::optional<RtpH264Depacketizer::Packet> parsed1 = depacketizer.Depacketize(CopyOnWriteBuffer(packet1));
    ASSERT_TRUE(parsed1);
    // We expect that the first packet is one byte shorter since the FU-A header
    // has been replaced by the original nal header.
    EXPECT_THAT(ArrayView(parsed1->video_payload.cdata(),
                          parsed1->video_payload.size()),
                ElementsAreArray(kExpected1));
    EXPECT_EQ(parsed1->video_header.frame_type, video::FrameType::KEY);
    EXPECT_EQ(parsed1->video_header.codec_type, Video::CodecType::H264);
    EXPECT_TRUE(parsed1->video_header.is_first_packet_in_frame);
    {
        const auto& h264 = std::get<h264::PacketizationInfo>(parsed1->video_codec_header);
        EXPECT_EQ(h264.packetization_type, h264::PacketizationType::FU_A);
        EXPECT_EQ(h264.packet_nalu_type, kIdr);
        ASSERT_EQ(h264.nalus.size(), 1u);
        EXPECT_EQ(h264.nalus[0].type, kIdr);
        EXPECT_EQ(h264.nalus[0].sps_id, -1);
        EXPECT_EQ(h264.nalus[0].pps_id, 0);
    }

    // Following packets will be 2 bytes shorter since they will only be appended
    // onto the first packet.
    auto parsed2 = depacketizer.Depacketize(CopyOnWriteBuffer(packet2));
    EXPECT_THAT(ArrayView(parsed2->video_payload.cdata(),
                          parsed2->video_payload.size()),
                ElementsAreArray(kExpected2));
    EXPECT_FALSE(parsed2->video_header.is_first_packet_in_frame);
    EXPECT_EQ(parsed2->video_header.codec_type, Video::CodecType::H264);
    {
        const auto& h264 = std::get<h264::PacketizationInfo>(parsed2->video_codec_header);
        EXPECT_EQ(h264.packetization_type, h264::PacketizationType::FU_A);
        EXPECT_EQ(h264.packet_nalu_type, kIdr);
        // NALU info is only expected for the first FU-A packet.
        EXPECT_EQ(h264.nalus.size(), 0u);
    }

    auto parsed3 = depacketizer.Depacketize(CopyOnWriteBuffer(packet3));
    EXPECT_THAT(ArrayView(parsed3->video_payload.cdata(),
                          parsed3->video_payload.size()),
                ElementsAreArray(kExpected3));
    EXPECT_FALSE(parsed3->video_header.is_first_packet_in_frame);
    EXPECT_EQ(parsed3->video_header.codec_type, Video::CodecType::H264);
    {
        const auto& h264 = std::get<h264::PacketizationInfo>(parsed3->video_codec_header);
        EXPECT_EQ(h264.packetization_type, h264::PacketizationType::FU_A);
        EXPECT_EQ(h264.packet_nalu_type, kIdr);
        // NALU info is only expected for the first FU-A packet.
        ASSERT_EQ(h264.nalus.size(), 0u);
    }
}

MY_TEST(RtpH264DepacketizerTest, EmptyPayload) {
    CopyOnWriteBuffer empty;
    RtpH264Depacketizer depacketizer;
    EXPECT_FALSE(depacketizer.Depacketize(empty));
}

MY_TEST(RtpH264DepacketizerTest, TruncatedFuaNalu) {
    const uint8_t kPayload[] = {0x9c};
    RtpH264Depacketizer depacketizer;
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(kPayload)));
}

MY_TEST(RtpH264DepacketizerTest, TruncatedSingleStapANalu) {
    const uint8_t kPayload[] = {0xd8, 0x27};
    RtpH264Depacketizer depacketizer;
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(kPayload)));
}

MY_TEST(RtpH264DepacketizerTest, StapAPacketWithTruncatedNalUnits) {
    const uint8_t kPayload[] = {0x58, 0xCB, 0xED, 0xDF};
    RtpH264Depacketizer depacketizer;
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(kPayload)));
}

MY_TEST(RtpH264DepacketizerTest, TruncationJustAfterSingleStapANalu) {
    const uint8_t kPayload[] = {0x38, 0x27, 0x27};
    RtpH264Depacketizer depacketizer;
    EXPECT_FALSE(depacketizer.Depacketize(CopyOnWriteBuffer(kPayload)));
}

MY_TEST(RtpH264DepacketizerTest, ShortSpsPacket) {
    const uint8_t kPayload[] = {0x27, 0x80, 0x00};
    RtpH264Depacketizer depacketizer;
    EXPECT_TRUE(depacketizer.Depacketize(CopyOnWriteBuffer(kPayload)));
}

MY_TEST(RtpH264DepacketizerTest, SeiPacket) {
    const uint8_t kPayload[] = {
        kSei,                   // F=0, NRI=0, Type=6.
        0x03, 0x03, 0x03, 0x03  // Payload.
    };
    RtpH264Depacketizer depacketizer;
    auto parsed = depacketizer.Depacketize(CopyOnWriteBuffer(kPayload));
    ASSERT_TRUE(parsed);
    const auto& h264 =
        std::get<h264::PacketizationInfo>(parsed->video_codec_header);
    EXPECT_EQ(parsed->video_header.frame_type, video::FrameType::DELTA);
    EXPECT_EQ(h264.packetization_type, h264::PacketizationType::SIGNLE);
    EXPECT_EQ(h264.packet_nalu_type, kSei);
    ASSERT_EQ(h264.nalus.size(), 1u);
    EXPECT_EQ(h264.nalus[0].type, kSei);
    EXPECT_EQ(h264.nalus[0].sps_id, -1);
    EXPECT_EQ(h264.nalus[0].pps_id, -1);
}

} // namespace test
} // namespace naivertc
