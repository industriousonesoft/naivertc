#include "rtc/rtp_rtcp/rtp/receiver/rtp_video_frame_assembler.hpp"
#include "common/array_view.hpp"
#include "common/utils_random.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

namespace naivertc {
namespace test {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Pointee;
using ::testing::SizeIs;

using Packet = RtpVideoFrameAssembler::Packet;
using InsertResult = RtpVideoFrameAssembler::InsertResult;

constexpr int kStartSize = 16;
constexpr int kMaxSize = 64;

std::vector<uint16_t> StartSeqNums(ArrayView<const std::unique_ptr<Packet>> packets) {
    std::vector<uint16_t> result;
    bool frame_boundary = true;
    for (const auto& packet : packets) {
        EXPECT_EQ(frame_boundary, packet->is_first_packet_in_frame());
        if (packet->is_first_packet_in_frame()) {
            result.push_back(packet->seq_num);
        }
        frame_boundary = packet->is_last_packet_in_frame();
    }
    EXPECT_TRUE(frame_boundary);
    return result;
}

MATCHER_P(StartSeqNumsAre, seq_num, "") {
    return Matches(ElementsAre(seq_num))(StartSeqNums(arg.assembled_packets));
}

MATCHER_P2(StartSeqNumsAre, seq_num1, seq_num2, "") {
  return Matches(ElementsAre(seq_num1, seq_num2))(StartSeqNums(arg.assembled_packets));
}

MATCHER(KeyFrame, "") {
  return arg->is_first_packet_in_frame() &&
         arg->video_header.frame_type == video::FrameType::KEY;
}

MATCHER(DeltaFrame, "") {
  return arg->is_first_packet_in_frame() &&
         arg->video_header.frame_type == video::FrameType::DELTA;
}

void PrintTo(const InsertResult& result, std::ostream& os) {
    os << "frames: { ";
    for (const auto& packet : result.assembled_packets) {
        if (packet->is_first_packet_in_frame() &&
            packet->is_last_packet_in_frame()) {
            os << "{sn: " << packet->seq_num << " }";
        } else if (packet->is_first_packet_in_frame()) {
            os << "{sn: [" << packet->seq_num << "-";
        } else if (packet->is_last_packet_in_frame()) {
            os << packet->seq_num << "] }, ";
        }
    }
    os << " }";
    if (result.keyframe_requested) {
        os << ", keyframe_requested";
    }
}

class PacketAssemblerTest : public ::testing::Test {
protected:
    PacketAssemblerTest() : frame_assembler_(kStartSize, kMaxSize) {}

    uint16_t Rand() { return utils::random::generate_random<uint16_t>(); }

    enum IsKeyFrame { kKeyFrame, kDeltaFrame };
    enum IsFirst { kFirst, kNotFirst };
    enum IsLast { kLast, kNotLast };

    InsertResult Insert(uint16_t seq_num,  // packet sequence number
                        IsKeyFrame keyframe,  // is keyframe
                        IsFirst first,  // is first packet of frame
                        IsLast last,    // is last packet of frame
                        ArrayView<const uint8_t> data = {},
                        uint32_t timestamp = 123u) {  // rtp timestamp
        auto packet = std::make_unique<Packet>();
        packet->video_header.codec_type = video::CodecType::H264;
        packet->timestamp = timestamp;
        packet->seq_num = seq_num;
        packet->video_header.frame_type = keyframe == kKeyFrame
                                            ? video::FrameType::KEY
                                            : video::FrameType::DELTA;
        packet->video_header.is_first_packet_in_frame = first == kFirst;
        packet->video_header.is_last_packet_in_frame = last == kLast;
        packet->video_payload.Assign(data.data(), data.size());

        return frame_assembler_.InsertPacket(std::move(packet));
    }

    RtpVideoFrameAssembler frame_assembler_;
};

TEST_F(PacketAssemblerTest, InsertOnePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
}

TEST_F(PacketAssemblerTest, InsertMultiplePackets) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 3, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
}

TEST_F(PacketAssemblerTest, InsertDuplicatePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast).assembled_packets, SizeIs(2));
}

TEST_F(PacketAssemblerTest, SeqNumWrapOneFrame) {
    Insert(0xFFFF, kKeyFrame, kFirst, kNotLast);
    EXPECT_THAT(Insert(0x0, kKeyFrame, kNotFirst, kLast), StartSeqNumsAre(0xFFFF));
}

TEST_F(PacketAssemblerTest, SeqNumWrapTwoFrames) {
    EXPECT_THAT(Insert(0xFFFF, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0xFFFF));
    EXPECT_THAT(Insert(0x0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0x0));
}

// TEST_F(PacketAssemblerTest, InsertOldPackets) {
//     EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
//     EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));
//     EXPECT_THAT(Insert(101, kKeyFrame, kNotFirst, kLast).assembled_packets, SizeIs(2));

//     EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
//     EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
//     EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));

//     frame_assembler_.ClearTo(102);
//     EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_packets, IsEmpty());
//     EXPECT_THAT(Insert(103, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));
// }
    
} // namespace test
} // namespace naivertc
