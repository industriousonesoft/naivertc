#include "rtc/media/video/codecs/h264/sps_pps_tracker.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {

using ::testing::ElementsAreArray;

const uint8_t start_code[] = {0, 0, 0, 1};

ArrayView<const uint8_t> Bitstream(const h264::SpsPpsTracker::FixedBitstream& fixed) {
    return fixed.bitstream;
}

void ExpectSpsPpsIdr(h264::PacketizationInfo& h264_header,
                     uint8_t sps_id,
                     uint8_t pps_id) {
    bool contains_sps = false;
    bool contains_pps = false;
    bool contains_idr = false;
    for (const auto& nalu : h264_header.nalus) {
        if (nalu.type == h264::NaluType::SPS) {
            EXPECT_EQ(sps_id, nalu.sps_id);
            contains_sps = true;
        } else if (nalu.type == h264::NaluType::PPS) {
            EXPECT_EQ(sps_id, nalu.sps_id);
            EXPECT_EQ(pps_id, nalu.pps_id);
            contains_pps = true;
        } else if (nalu.type == h264::NaluType::IDR) {
            EXPECT_EQ(pps_id, nalu.pps_id);
            contains_idr = true;
        }
    }
    EXPECT_TRUE(contains_sps);
    EXPECT_TRUE(contains_pps);
    EXPECT_TRUE(contains_idr);
}

class H264VideoHeader {
public:
    H264VideoHeader() {
        video_header.codec_type = VideoCodecType::H264;
    }

    RtpVideoHeader video_header;
    h264::PacketizationInfo h264_header;
};

class T(SpsPpsTrackerTest) : public ::testing::Test {
public:
    h264::SpsPpsTracker::FixedBitstream CopyAndFixBitstream(ArrayView<const uint8_t> bitstream, H264VideoHeader& header) {
        return tracker_.CopyAndFixBitstream(header.video_header.is_first_packet_in_frame, 
                                            header.video_header.frame_width, 
                                            header.video_header.frame_height, 
                                            header.h264_header, 
                                            bitstream);
    }

    void AddIdr(H264VideoHeader& header, int pps_id) {
        h264::NaluInfo info;
        info.type = h264::NaluType::IDR;
        info.sps_id = -1;
        info.pps_id = pps_id;
        header.h264_header.nalus.push_back(std::move(info));
    }

    void AddSps(H264VideoHeader& header, uint8_t sps_id, std::vector<uint8_t>& data) {
        h264::NaluInfo info;
        info.type = h264::NaluType::SPS;
        info.sps_id = sps_id;
        info.pps_id = -1;
        data.push_back(h264::NaluType::SPS);
        data.push_back(sps_id);

        header.h264_header.nalus.push_back(std::move(info));
    }

    void AddPps(H264VideoHeader& header, uint8_t sps_id, uint8_t pps_id, std::vector<uint8_t>& data) {
        h264::NaluInfo info;
        info.type = h264::NaluType::PPS;
        info.sps_id = sps_id;
        info.pps_id = pps_id;
        data.push_back(h264::NaluType::PPS);
        data.push_back(pps_id);
        header.h264_header.nalus.push_back(std::move(info));
    }

    void InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                           const std::vector<uint8_t>& pps) {
        tracker_.InsertSpsPpsNalus(sps, pps);
    }

private:
    h264::SpsPpsTracker tracker_;
};

MY_TEST_F(SpsPpsTrackerTest, NoNalus) {
    uint8_t data[] = {1, 2, 3};
    H264VideoHeader header;
    header.h264_header.packetization_type = h264::PacketizationType::FU_A;
    auto fixed = CopyAndFixBitstream(data, header);
    EXPECT_EQ(fixed.action, h264::SpsPpsTracker::PacketAction::INSERT);
    EXPECT_THAT(Bitstream(fixed), ElementsAreArray(data));
}

MY_TEST_F(SpsPpsTrackerTest, FuAFirstPacket) {
    uint8_t data[] = {1, 2, 3};
    H264VideoHeader header;
    header.video_header.is_first_packet_in_frame = true;

    header.h264_header.packetization_type = h264::PacketizationType::FU_A;
    header.h264_header.nalus.resize(1);
    
    h264::SpsPpsTracker::FixedBitstream fixed = CopyAndFixBitstream(data, header);

    EXPECT_EQ(fixed.action, h264::SpsPpsTracker::PacketAction::INSERT);
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
    expected.insert(expected.end(), {1, 2, 3});
    EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

MY_TEST_F(SpsPpsTrackerTest, StapAIncorrectSegmentLength) {
    uint8_t data[] = {0, 0, 2, 0};
    H264VideoHeader header;
    header.video_header.is_first_packet_in_frame = true;

    header.h264_header.packetization_type = h264::PacketizationType::STAP_A;
   
    EXPECT_EQ(CopyAndFixBitstream(data, header).action,
              h264::SpsPpsTracker::PacketAction::DROP);
}

MY_TEST_F(SpsPpsTrackerTest, SingleNaluInsertStartCode) {
    uint8_t data[] = {1, 2, 3};
    H264VideoHeader header;
    header.h264_header.nalus.resize(1);

    h264::SpsPpsTracker::FixedBitstream fixed = CopyAndFixBitstream(data, header);

    EXPECT_EQ(fixed.action, h264::SpsPpsTracker::PacketAction::INSERT);
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
    expected.insert(expected.end(), {1, 2, 3});
    EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

MY_TEST_F(SpsPpsTrackerTest, NoStartCodeInsertedForSubsequentFuAPacket) {
    std::vector<uint8_t> data = {1, 2, 3};
    H264VideoHeader header;
    header.h264_header.packetization_type = h264::PacketizationType::FU_A;
    // Since no NALU begin in this packet the nalus_length is zero.
    EXPECT_EQ(header.h264_header.nalus.size(), 0);

    h264::SpsPpsTracker::FixedBitstream fixed = CopyAndFixBitstream(data, header);

    EXPECT_EQ(fixed.action, h264::SpsPpsTracker::PacketAction::INSERT);
    EXPECT_THAT(Bitstream(fixed), ElementsAreArray(data));
}

MY_TEST_F(SpsPpsTrackerTest, IdrFirstPacketNoSpsPpsInserted) {
    std::vector<uint8_t> data = {1, 2, 3};
    H264VideoHeader header;
    header.video_header.is_first_packet_in_frame = true;
    AddIdr(header, 0);

    EXPECT_EQ(CopyAndFixBitstream(data, header).action,
              h264::SpsPpsTracker::PacketAction::REQUEST_KEY_FRAME);
}

MY_TEST_F(SpsPpsTrackerTest, IdrFirstPacketNoPpsInserted) {
    std::vector<uint8_t> data = {1, 2, 3};
    H264VideoHeader header;
    header.video_header.is_first_packet_in_frame = true;
    AddSps(header, 0, data);
    AddIdr(header, 0);

    EXPECT_EQ(CopyAndFixBitstream(data, header).action,
              h264::SpsPpsTracker::PacketAction::REQUEST_KEY_FRAME);
}

MY_TEST_F(SpsPpsTrackerTest, IdrFirstPacketNoSpsInserted) {
    std::vector<uint8_t> data = {1, 2, 3};
    H264VideoHeader header;
    header.video_header.is_first_packet_in_frame = true;
    AddPps(header, 0, 0, data);
    AddIdr(header, 0);

    EXPECT_EQ(CopyAndFixBitstream(data, header).action,
              h264::SpsPpsTracker::PacketAction::REQUEST_KEY_FRAME);
}

MY_TEST_F(SpsPpsTrackerTest, SpsPpsPacketThenIdrFirstPacket) {
    std::vector<uint8_t> data;
    H264VideoHeader sps_pps_header;
    // Insert SPS/PPS as a single NAL unit
    AddSps(sps_pps_header, 0, data);
    AddPps(sps_pps_header, 0, 1, data);

    EXPECT_EQ(CopyAndFixBitstream(data, sps_pps_header).action,
              h264::SpsPpsTracker::PacketAction::INSERT);

    // Insert first packet of the IDR
    H264VideoHeader idr_header;
    idr_header.video_header.is_first_packet_in_frame = true;
    AddIdr(idr_header, 1);
    data = {1, 2, 3};

    h264::SpsPpsTracker::FixedBitstream fixed = CopyAndFixBitstream(data, idr_header);
    EXPECT_EQ(fixed.action, h264::SpsPpsTracker::PacketAction::INSERT);

    std::vector<uint8_t> expected;
    expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
    expected.insert(expected.end(), {1, 2, 3});
    EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

MY_TEST_F(SpsPpsTrackerTest, SpsPpsIdrInStapA) {
    std::vector<uint8_t> data;
    H264VideoHeader header;
    header.h264_header.packetization_type = h264::PacketizationType::STAP_A;
    header.video_header.is_first_packet_in_frame = true;  // Always true for StapA

    data.insert(data.end(), {0});     // First byte is ignored
    data.insert(data.end(), {0, 2});  // Length of segment
    AddSps(header, 13, data);
    data.insert(data.end(), {0, 2});  // Length of segment
    AddPps(header, 13, 27, data);
    data.insert(data.end(), {0, 5});  // Length of segment
    AddIdr(header, 27);
    data.insert(data.end(), {1, 2, 3, 2, 1});

    h264::SpsPpsTracker::FixedBitstream fixed = CopyAndFixBitstream(data, header);

    EXPECT_THAT(fixed.action, h264::SpsPpsTracker::PacketAction::INSERT);

    std::vector<uint8_t> expected;
    expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
    expected.insert(expected.end(), {h264::NaluType::SPS, 13});
    expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
    expected.insert(expected.end(), {h264::NaluType::PPS, 27});
    expected.insert(expected.end(), start_code, start_code + sizeof(start_code));
    expected.insert(expected.end(), {1, 2, 3, 2, 1});
    EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

MY_TEST_F(SpsPpsTrackerTest, SpsPpsOutOfBand) {
    constexpr uint8_t kData[] = {1, 2, 3};

    // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
    // width: 320, height: 240
    const std::vector<uint8_t> sps(
        {0x67, 0x7a, 0x00, 0x0d, 0xbc, 0xd9, 0x41, 0x41, 0xfa, 0x10, 0x00, 0x00,
         0x03, 0x00, 0x10, 0x00, 0x00, 0x03, 0x03, 0xc0, 0xf1, 0x42, 0x99, 0x60});
    const std::vector<uint8_t> pps({0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
    InsertSpsPpsNalus(sps, pps);

    // Insert first packet of the IDR.
    H264VideoHeader idr_header;
    idr_header.video_header.is_first_packet_in_frame = true;
    AddIdr(idr_header, 0);
    EXPECT_EQ(idr_header.h264_header.nalus.size(), 1u);

    h264::SpsPpsTracker::FixedBitstream fixed = CopyAndFixBitstream(kData, idr_header);

    EXPECT_EQ(idr_header.h264_header.nalus.size(), 3u);
    EXPECT_EQ(idr_header.video_header.frame_width, 320u);
    EXPECT_EQ(idr_header.video_header.frame_height, 240u);
    ExpectSpsPpsIdr(idr_header.h264_header, 0, 0);
}

MY_TEST_F(SpsPpsTrackerTest, SpsPpsOutOfBandWrongNaluHeader) {
    constexpr uint8_t kData[] = {1, 2, 3};

    // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
    // Nalu headers manupilated afterwards.
    const std::vector<uint8_t> sps(
        {0xff, 0x7a, 0x00, 0x0d, 0xbc, 0xd9, 0x41, 0x41, 0xfa, 0x10, 0x00, 0x00,
        0x03, 0x00, 0x10, 0x00, 0x00, 0x03, 0x03, 0xc0, 0xf1, 0x42, 0x99, 0x60});
    const std::vector<uint8_t> pps({0xff, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
    InsertSpsPpsNalus(sps, pps);

    // Insert first packet of the IDR.
    H264VideoHeader idr_header;
    idr_header.video_header.is_first_packet_in_frame = true;
    AddIdr(idr_header, 0);

    EXPECT_EQ(CopyAndFixBitstream(kData, idr_header).action,
                h264::SpsPpsTracker::PacketAction::REQUEST_KEY_FRAME);
}

MY_TEST_F(SpsPpsTrackerTest, SpsPpsOutOfBandIncompleteNalu) {
    constexpr uint8_t kData[] = {1, 2, 3};

    // Generated by "ffmpeg -r 30 -f avfoundation -i "default" out.h264" on macos.
    // Nalus damaged afterwards.
    const std::vector<uint8_t> sps({0x67, 0x7a, 0x00, 0x0d, 0xbc, 0xd9});
    const std::vector<uint8_t> pps({0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0});
    InsertSpsPpsNalus(sps, pps);

    // Insert first packet of the IDR.
    H264VideoHeader idr_header;
    idr_header.video_header.is_first_packet_in_frame = true;
    AddIdr(idr_header, 0);

    EXPECT_EQ(CopyAndFixBitstream(kData, idr_header).action,
              h264::SpsPpsTracker::PacketAction::REQUEST_KEY_FRAME);
}

MY_TEST_F(SpsPpsTrackerTest, SaveRestoreWidthHeight) {
    std::vector<uint8_t> data;

    // Insert an SPS/PPS packet with width/height and make sure
    // that information is set on the first IDR packet.
    H264VideoHeader sps_pps_header;
    AddSps(sps_pps_header, 0, data);
    AddPps(sps_pps_header, 0, 1, data);
    sps_pps_header.video_header.frame_width = 320;
    sps_pps_header.video_header.frame_height = 240;

    EXPECT_EQ(CopyAndFixBitstream(data, sps_pps_header).action,
              h264::SpsPpsTracker::PacketAction::INSERT);

    H264VideoHeader idr_header;
    idr_header.video_header.is_first_packet_in_frame = true;
    AddIdr(idr_header, 1);
    data.insert(data.end(), {1, 2, 3});

    EXPECT_EQ(CopyAndFixBitstream(data, idr_header).action,
              h264::SpsPpsTracker::PacketAction::INSERT);

    EXPECT_EQ(idr_header.video_header.frame_width, 320);
    EXPECT_EQ(idr_header.video_header.frame_height, 240);
}

    
} // namespace test
} // namespace naivertc
