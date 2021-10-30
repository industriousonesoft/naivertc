#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer.hpp"
#include "common/array_view.hpp"
#include "common/utils_random.hpp"
#include "rtc/rtp_rtcp/components/seq_num_unwrapper.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <variant>

namespace naivertc {
namespace test {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Pointee;
using ::testing::SizeIs;

using Packet = rtc::video::jitter::PacketBuffer::Packet;
using Frame = rtc::video::jitter::PacketBuffer::Frame;
using InsertResult = rtc::video::jitter::PacketBuffer::InsertResult;

constexpr int kStartSize = 16;
constexpr int kMaxSize = 64;

std::vector<uint16_t> StartSeqNums(ArrayView<const std::unique_ptr<Frame>> frames) {
    std::vector<uint16_t> result;
    for (const auto& frame : frames) {
        result.push_back(frame->seq_num_start);
    }
    return result;
}

size_t NumPackets(ArrayView<const std::unique_ptr<Frame>> frames) {
    size_t num_packets = 0;
    for (const auto& frame : frames) {
        num_packets += frame->num_packets;
    }
    return num_packets;
}

MATCHER_P(StartSeqNumsAre, seq_num, "") {
    return Matches(ElementsAre(seq_num))(StartSeqNums(arg.assembled_frames));
}

MATCHER_P2(StartSeqNumsAre, seq_num1, seq_num2, "") {
  return Matches(ElementsAre(seq_num1, seq_num2))(StartSeqNums(arg.assembled_frames));
}

MATCHER(KeyFrame, "") {
  return arg->frame_type == VideoFrameType::KEY;
}

MATCHER(DeltaFrame, "") {
  return arg->frame_type == VideoFrameType::DELTA;
}

void PrintTo(const InsertResult& result, std::ostream& os) {
    os << "frames: { ";
    for (const auto& frame : result.assembled_frames) {
        if (frame->seq_num_start == frame->seq_num_end) {
            os << "{sn: " << frame->seq_num_start << " }";
        } else {
            os << "{sn: [" << frame->seq_num_start << "-" << frame->seq_num_end << "] }, ";
        }
    }
    os << " }";
    if (result.keyframe_requested) {
        os << ", keyframe_requested";
    }
}

class VCM_PacketBufferTest : public ::testing::Test {
protected:
    VCM_PacketBufferTest() : packet_buffer_(kStartSize, kMaxSize) {}

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
        packet->video_header.codec_type = VideoCodecType::GENERIC;
        packet->timestamp = timestamp;
        packet->seq_num = seq_num;
        packet->video_header.frame_type = keyframe == kKeyFrame
                                            ? VideoFrameType::KEY
                                            : VideoFrameType::DELTA;
        packet->video_header.is_first_packet_in_frame = first == kFirst;
        packet->video_header.is_last_packet_in_frame = last == kLast;
        packet->video_payload.Assign(data.data(), data.size());

        return packet_buffer_.InsertPacket(std::move(packet));
    }

    rtc::video::jitter::PacketBuffer packet_buffer_;
};

TEST_F(VCM_PacketBufferTest, InsertOnePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).assembled_frames, SizeIs(1));
}

TEST_F(VCM_PacketBufferTest, InsertMultiplePackets) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).assembled_frames, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast).assembled_frames, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kFirst, kLast).assembled_frames, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 3, kKeyFrame, kFirst, kLast).assembled_frames, SizeIs(1));
}

TEST_F(VCM_PacketBufferTest, InsertDuplicatePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    auto frames = Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast).assembled_frames;
    EXPECT_THAT(frames, SizeIs(1));
    EXPECT_EQ(NumPackets(frames), 2);
}

TEST_F(VCM_PacketBufferTest, SeqNumWrapOneFrame) {
    Insert(0xFFFF, kKeyFrame, kFirst, kNotLast);
    auto ret = Insert(0x00, kKeyFrame, kNotFirst, kLast);
    EXPECT_THAT(ret.assembled_frames, SizeIs(1));
    EXPECT_EQ(NumPackets(ret.assembled_frames), 2);
    EXPECT_THAT(ret, StartSeqNumsAre(0xFFFF));
}

TEST_F(VCM_PacketBufferTest, SeqNumWrapTwoFrames) {
    EXPECT_THAT(Insert(0xFFFF, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0xFFFF));
    EXPECT_THAT(Insert(0x0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0x0));
}

TEST_F(VCM_PacketBufferTest, InsertOldPackets) {
    EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_frames, SizeIs(1));
    auto frames = Insert(101, kKeyFrame, kNotFirst, kLast).assembled_frames;
    EXPECT_THAT(frames, SizeIs(1));
    EXPECT_EQ(NumPackets(frames), 2);

    EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_frames, SizeIs(1));

    packet_buffer_.ClearTo(102);
    EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(103, kDeltaFrame, kFirst, kLast).assembled_frames, SizeIs(1));
}

TEST_F(VCM_PacketBufferTest, FrameSize) {
    const uint16_t seq_num = Rand();
    uint8_t data1[5] = {};
    uint8_t data2[5] = {};
    uint8_t data3[5] = {};
    uint8_t data4[5] = {};

    Insert(seq_num, kKeyFrame, kFirst, kNotLast, data1);
    Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, data2);
    Insert(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, data3);
    auto frames = Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, data4).assembled_frames;
    // Expect one frame of 4 packets.
    EXPECT_THAT(StartSeqNums(frames), ElementsAre(seq_num));
    EXPECT_THAT(frames, SizeIs(1));
    EXPECT_EQ(NumPackets(frames), 4);
}

TEST_F(VCM_PacketBufferTest, ExpandBuffer) {
    const uint16_t seq_num = Rand();

    Insert(seq_num, kKeyFrame, kFirst, kNotLast);
    for (int i = 1; i < kStartSize; ++i)
        EXPECT_FALSE(Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast).keyframe_requested);

    // Already inserted kStartSize number of packets, inserting the last packet
    // should increase the buffer size and also result in an assembled frame.
    EXPECT_FALSE(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast).keyframe_requested);
}

TEST_F(VCM_PacketBufferTest, SingleFrameExpandsBuffer) {
    const uint16_t seq_num = Rand();

    Insert(seq_num, kKeyFrame, kFirst, kNotLast);
    for (int i = 1; i < kStartSize; ++i)
        Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast), StartSeqNumsAre(seq_num));
}

TEST_F(VCM_PacketBufferTest, ExpandBufferOverflow) {
    const uint16_t seq_num = Rand();

    EXPECT_FALSE(Insert(seq_num, kKeyFrame, kFirst, kNotLast).keyframe_requested);
    for (int i = 1; i < kMaxSize; ++i)
        EXPECT_FALSE(Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast).keyframe_requested);

    // Already inserted kMaxSize number of packets, inserting the last packet
    // should overflow the buffer and result in false being returned.
    EXPECT_TRUE(Insert(seq_num + kMaxSize, kKeyFrame, kNotFirst, kLast).keyframe_requested);
}


TEST_F(VCM_PacketBufferTest, OnePacketOneFrame) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast), StartSeqNumsAre(seq_num));
}

TEST_F(VCM_PacketBufferTest, TwoPacketsTwoFrames) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast), StartSeqNumsAre(seq_num));
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast), StartSeqNumsAre(seq_num + 1));
}

TEST_F(VCM_PacketBufferTest, TwoPacketsOneFrames) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast), StartSeqNumsAre(seq_num));
}

TEST_F(VCM_PacketBufferTest, ThreePacketReorderingOneFrame) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kNotFirst, kLast).assembled_frames, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast), StartSeqNumsAre(seq_num));
}

TEST_F(VCM_PacketBufferTest, Frames) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast), 
                StartSeqNumsAre(seq_num));
    EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast), 
                StartSeqNumsAre(seq_num + 1));
    EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast), 
                StartSeqNumsAre(seq_num + 2));
    EXPECT_THAT(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast), 
                StartSeqNumsAre(seq_num + 3));
}

TEST_F(VCM_PacketBufferTest, ClearSinglePacket) {
    const uint16_t seq_num = Rand();

    for (int i = 0; i < kMaxSize; ++i)
        Insert(seq_num + i, kDeltaFrame, kFirst, kLast);

    packet_buffer_.ClearTo(seq_num);
    EXPECT_FALSE(Insert(seq_num + kMaxSize, kDeltaFrame, kFirst, kLast).keyframe_requested);
}

TEST_F(VCM_PacketBufferTest, ClearFullBuffer) {
    for (int i = 0; i < kMaxSize; ++i)
        Insert(i, kDeltaFrame, kFirst, kLast);

    packet_buffer_.ClearTo(kMaxSize - 1);

    for (int i = kMaxSize; i < 2 * kMaxSize; ++i)
        EXPECT_FALSE(Insert(i, kDeltaFrame, kFirst, kLast).keyframe_requested);
}

TEST_F(VCM_PacketBufferTest, DontClearNewerPacket) {
    EXPECT_THAT(Insert(0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0));
    packet_buffer_.ClearTo(0);
    EXPECT_THAT(Insert(2 * kStartSize, kKeyFrame, kFirst, kLast),
                StartSeqNumsAre(2 * kStartSize));
    EXPECT_THAT(Insert(3 * kStartSize + 1, kKeyFrame, kFirst, kNotLast).assembled_frames,
                IsEmpty());
    packet_buffer_.ClearTo(2 * kStartSize);
    EXPECT_THAT(Insert(3 * kStartSize + 2, kKeyFrame, kNotFirst, kLast),
                StartSeqNumsAre(3 * kStartSize + 1));
}

TEST_F(VCM_PacketBufferTest, OneIncompleteFrame) {
    const uint16_t seq_num = Rand();

    EXPECT_THAT(Insert(seq_num, kDeltaFrame, kFirst, kNotLast).assembled_frames,
                IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kLast),
                StartSeqNumsAre(seq_num));
    EXPECT_THAT(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast).assembled_frames,
                IsEmpty());
}

TEST_F(VCM_PacketBufferTest, TwoIncompleteFramesFullBuffer) {
    const uint16_t seq_num = Rand();

    for (int i = 1; i < kMaxSize - 1; ++i)
        Insert(seq_num + i, kDeltaFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num, kDeltaFrame, kFirst, kNotLast).assembled_frames,
                IsEmpty());
    EXPECT_THAT(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast).assembled_frames,
                IsEmpty());
}

TEST_F(VCM_PacketBufferTest, FramesReordered) {
    const uint16_t seq_num = Rand();

    EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast),
                StartSeqNumsAre(seq_num + 1));
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast),
                StartSeqNumsAre(seq_num));
    EXPECT_THAT(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast),
                StartSeqNumsAre(seq_num + 3));
    EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast),
                StartSeqNumsAre(seq_num + 2));
}

TEST_F(VCM_PacketBufferTest, InsertPacketAfterSequenceNumberWrapAround) {
    uint16_t kFirstSeqNum = 0;
    uint32_t kTimestampDelta = 100;
    uint32_t timestamp = 10000;
    uint16_t seq_num = kFirstSeqNum;

    // Loop until seq_num wraps around.
    SeqNumUnwrapper<uint16_t> unwrapper;
    while (unwrapper.Unwrap(seq_num) < std::numeric_limits<uint16_t>::max()) {
        Insert(seq_num++, kKeyFrame, kFirst, kNotLast, {}, timestamp);
        for (int i = 0; i < 5; ++i) {
            Insert(seq_num++, kKeyFrame, kNotFirst, kNotLast, {}, timestamp);
        }
        Insert(seq_num++, kKeyFrame, kNotFirst, kLast, {}, timestamp);
        timestamp += kTimestampDelta;
    }

    // Receive frame with overlapping sequence numbers.
    Insert(seq_num++, kKeyFrame, kFirst, kNotLast, {}, timestamp);
    for (int i = 0; i < 5; ++i) {
        Insert(seq_num++, kKeyFrame, kNotFirst, kNotLast, {}, timestamp);
    }
    auto frames = Insert(seq_num++, kKeyFrame, kNotFirst, kLast, {}, timestamp).assembled_frames;
    // One frame of 7 packets.
    EXPECT_THAT(StartSeqNums(frames), SizeIs(1));
    EXPECT_THAT(frames, SizeIs(1));
    EXPECT_THAT(NumPackets(frames), 7);
}

TEST_F(VCM_PacketBufferTest, FreeSlotsOnFrameCreation) {
    const uint16_t seq_num = Rand();

    Insert(seq_num, kKeyFrame, kFirst, kNotLast);
    Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast),
                StartSeqNumsAre(seq_num));

    // Insert frame that fills the whole buffer.
    Insert(seq_num + 3, kKeyFrame, kFirst, kNotLast);
    for (int i = 0; i < kMaxSize - 2; ++i)
        Insert(seq_num + i + 4, kDeltaFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num + kMaxSize + 2, kKeyFrame, kNotFirst, kLast),
                StartSeqNumsAre(seq_num + 3));
}

TEST_F(VCM_PacketBufferTest, Clear) {
    const uint16_t seq_num = Rand();

    Insert(seq_num, kKeyFrame, kFirst, kNotLast);
    Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast),
                StartSeqNumsAre(seq_num));

    packet_buffer_.Clear();

    Insert(seq_num + kStartSize, kKeyFrame, kFirst, kNotLast);
    Insert(seq_num + kStartSize + 1, kDeltaFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num + kStartSize + 2, kDeltaFrame, kNotFirst, kLast),
                StartSeqNumsAre(seq_num + kStartSize));
}

TEST_F(VCM_PacketBufferTest, FramesAfterClear) {
    Insert(9025, kDeltaFrame, kFirst, kLast);
    Insert(9024, kKeyFrame, kFirst, kLast);
    packet_buffer_.ClearTo(9025);
    EXPECT_THAT(Insert(9057, kDeltaFrame, kFirst, kLast).assembled_frames, SizeIs(1));
    EXPECT_THAT(Insert(9026, kDeltaFrame, kFirst, kLast).assembled_frames, SizeIs(1));
}

TEST_F(VCM_PacketBufferTest, SameFrameDifferentTimestamps) {
    Insert(0, kKeyFrame, kFirst, kNotLast, {}, 1000);
    EXPECT_THAT(Insert(1, kKeyFrame, kNotFirst, kLast, {}, 1001).assembled_frames,
                IsEmpty());
}

TEST_F(VCM_PacketBufferTest, ContinuousSeqNumDoubleMarkerBit) {
    Insert(2, kKeyFrame, kNotFirst, kNotLast);
    Insert(1, kKeyFrame, kFirst, kLast);
    EXPECT_THAT(Insert(3, kKeyFrame, kNotFirst, kLast).assembled_frames, IsEmpty());
}

TEST_F(VCM_PacketBufferTest, TooManyNalusInPacket) {
    auto packet = std::make_unique<Packet>();
    packet->video_header.codec_type = VideoCodecType::H264;
    packet->timestamp = 1;
    packet->seq_num = 1;
    packet->video_header.frame_type = VideoFrameType::KEY;
    packet->video_header.is_first_packet_in_frame = true;
    packet->video_header.is_last_packet_in_frame = true;
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(h264::kMaxNaluNumPerPacket);
    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_frames,
                IsEmpty());
}

// If |sps_pps_idr_is_keyframe| is true, we require keyframes to contain
// SPS/PPS/IDR and the keyframes we create as part of the test do contain
// SPS/PPS/IDR. If |sps_pps_idr_is_keyframe| is false, we only require and
// create keyframes containing only IDR.
class PacketBufferH264Test : public VCM_PacketBufferTest {
protected:
    explicit PacketBufferH264Test(bool sps_pps_idr_is_keyframe)
        : VCM_PacketBufferTest() {
        packet_buffer_.set_sps_pps_idr_is_h264_keyframe(sps_pps_idr_is_keyframe);
    }

    InsertResult InsertH264(uint16_t seq_num,     // packet sequence number
                            IsKeyFrame keyframe,  // is keyframe
                            IsFirst first,        // is first packet of frame
                            IsLast last,          // is last packet of frame
                            uint32_t timestamp,   // rtp timestamp
                            ArrayView<const uint8_t> data = {},
                            uint32_t width = 0,     // width of frame (SPS/IDR)
                            uint32_t height = 0) {  // height of frame (SPS/IDR)
        auto packet = std::make_unique<Packet>();
        packet->video_header.codec_type = VideoCodecType::H264;
        auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
        
        packet->seq_num = seq_num;
        packet->timestamp = timestamp;
        if (keyframe == kKeyFrame) {
            if (packet_buffer_.sps_pps_idr_is_h264_keyframe()) {
                h264_header.nalus.resize(3);
                h264_header.nalus[0].type = h264::NaluType::SPS;
                h264_header.nalus[1].type = h264::NaluType::PPS;
                h264_header.nalus[2].type = h264::NaluType::IDR;
                h264_header.has_sps = true;
                h264_header.has_pps = true;
                h264_header.has_idr = true;
            } else {
                h264_header.nalus.resize(1);
                h264_header.nalus[0].type = h264::NaluType::IDR;
                h264_header.has_sps = false;
                h264_header.has_pps = false;
                h264_header.has_idr = true;
            }
        }
        packet->video_header.frame_width = width;
        packet->video_header.frame_height = height;
        packet->video_header.is_first_packet_in_frame = first == kFirst;
        packet->video_header.is_last_packet_in_frame = last == kLast;
        packet->video_payload.Assign(data.data(), data.size());

        return packet_buffer_.InsertPacket(std::move(packet));
    }

};

// This fixture is used to test the general behaviour of the packet buffer
// in both configurations.
class VCM_PacketBufferH264ParameterizedTest
    : public ::testing::WithParamInterface<bool>,
      public PacketBufferH264Test {
protected:
    VCM_PacketBufferH264ParameterizedTest() : PacketBufferH264Test(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(SpsPpsIdrIsKeyframe,
                         VCM_PacketBufferH264ParameterizedTest,
                         ::testing::Bool());

TEST_P(VCM_PacketBufferH264ParameterizedTest, DontRemoveMissingPacketOnClearTo) {
    InsertH264(0, kKeyFrame, kFirst, kLast, 0);
    InsertH264(2, kDeltaFrame, kFirst, kNotLast, 2);
    packet_buffer_.ClearTo(0);
    // Expect no frame because of missing of packet #1
    EXPECT_THAT(InsertH264(3, kDeltaFrame, kNotFirst, kLast, 2).assembled_frames,
                IsEmpty());
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, GetBitstreamOneFrameFullBuffer) {
    uint8_t data_arr[kStartSize][1];
    uint8_t expected[kStartSize];

    for (uint8_t i = 0; i < kStartSize; ++i) {
        data_arr[i][0] = i;
        expected[i] = i;
    }

    InsertH264(0, kKeyFrame, kFirst, kNotLast, 1, data_arr[0]);
    for (uint8_t i = 1; i < kStartSize - 1; ++i) {
        InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1, data_arr[i]);
    }

    auto frames = InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1, data_arr[kStartSize - 1]).assembled_frames;
    ASSERT_THAT(frames, SizeIs(1));
    ASSERT_THAT(StartSeqNums(frames), ElementsAre(0));
    EXPECT_EQ(frames[0]->num_packets, kStartSize);
    EXPECT_EQ(frames[0]->bitstream.size(), kStartSize);
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, GetBitstreamBufferPadding) {
    uint16_t seq_num = Rand();
    CopyOnWriteBuffer data = "some plain old data";

    auto packet = std::make_unique<Packet>();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(1);
    h264_header.nalus[0].type = h264::NaluType::IDR;
    h264_header.packetization_type = h264::PacketizationType::SIGNLE;
    packet->seq_num = seq_num;
    packet->video_header.codec_type = VideoCodecType::H264;
    packet->video_payload = data;
    packet->video_header.is_first_packet_in_frame = true;
    packet->video_header.is_last_packet_in_frame = true;
    auto frames = packet_buffer_.InsertPacket(std::move(packet)).assembled_frames;

    ASSERT_THAT(frames, SizeIs(1));
    EXPECT_EQ(frames[0]->seq_num_start, seq_num);
    EXPECT_EQ(frames[0]->seq_num_end, seq_num);
    EXPECT_EQ(frames[0]->bitstream, data);
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, FrameResolution) {
    uint16_t seq_num = 100;
    uint8_t data[] = "some plain old data";
    uint32_t width = 640;
    uint32_t height = 360;
    uint32_t timestamp = 1000;

    auto frames = InsertH264(seq_num, kKeyFrame, kFirst, kLast, timestamp, data, width, height).assembled_frames;

    ASSERT_THAT(frames, SizeIs(1));
    EXPECT_EQ(frames[0]->frame_width, width);
    EXPECT_EQ(frames[0]->frame_height, height);
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, OneFrameFillBuffer) {
    InsertH264(0, kKeyFrame, kFirst, kNotLast, 1000);
    for (int i = 1; i < kStartSize - 1; ++i)
        InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1000);
    EXPECT_THAT(InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1000),
                StartSeqNumsAre(0));
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, CreateFramesAfterFilledBuffer) {
    EXPECT_THAT(InsertH264(kStartSize - 2, kKeyFrame, kFirst, kLast, 0).assembled_frames,
                SizeIs(1));

    InsertH264(kStartSize, kDeltaFrame, kFirst, kNotLast, 2000);
    for (int i = 1; i < kStartSize; ++i)
        InsertH264(kStartSize + i, kDeltaFrame, kNotFirst, kNotLast, 2000);

    EXPECT_THAT(InsertH264(kStartSize + kStartSize, kDeltaFrame, kNotFirst, kLast, 2000).assembled_frames,
                IsEmpty());

    EXPECT_THAT(InsertH264(kStartSize - 1, kKeyFrame, kFirst, kLast, 1000),
                StartSeqNumsAre(kStartSize - 1, kStartSize));
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, OneFrameMaxSeqNum) {
    InsertH264(65534, kKeyFrame, kFirst, kNotLast, 1000);
    EXPECT_THAT(InsertH264(65535, kKeyFrame, kNotFirst, kLast, 1000),
                StartSeqNumsAre(65534));
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, ClearMissingPacketsOnKeyframe) {
    EXPECT_THAT(InsertH264(0, kKeyFrame, kFirst, kLast, 1000), StartSeqNumsAre(0));
    EXPECT_THAT(InsertH264(2, kKeyFrame, kFirst, kLast, 3000).assembled_frames, SizeIs(1));
    EXPECT_THAT(InsertH264(3, kDeltaFrame, kFirst, kNotLast, 4000).assembled_frames, SizeIs(0));

    auto frames = InsertH264(4, kDeltaFrame, kNotFirst, kLast, 4000).assembled_frames;
    EXPECT_THAT(frames, SizeIs(1));
    EXPECT_EQ(NumPackets(frames), 2);

    auto ret = InsertH264(10, kKeyFrame, kFirst, kLast, 18000);
    EXPECT_EQ(ret.assembled_frames.size(), 1);
    EXPECT_THAT(InsertH264(kStartSize + 1, kKeyFrame, kFirst, kLast, 18000),
                StartSeqNumsAre(kStartSize + 1));
}

TEST_P(VCM_PacketBufferH264ParameterizedTest, FindFramesOnPadding) {
    EXPECT_THAT(InsertH264(0, kKeyFrame, kFirst, kLast, 1000), StartSeqNumsAre(0));
    EXPECT_THAT(InsertH264(2, kDeltaFrame, kFirst, kLast, 1000).assembled_frames,
                IsEmpty());
    EXPECT_THAT(packet_buffer_.InsertPadding(1), StartSeqNumsAre(2));
}

class PacketBufferH264XIsKeyframeTest : public PacketBufferH264Test {
protected:
    const uint16_t kSeqNum = 5;

    explicit PacketBufferH264XIsKeyframeTest(bool sps_pps_idr_is_keyframe)
        : PacketBufferH264Test(sps_pps_idr_is_keyframe) {}

    std::unique_ptr<Packet> CreatePacket() {
        auto packet = std::make_unique<Packet>();
        packet->video_header.codec_type = VideoCodecType::H264;
        packet->seq_num = kSeqNum;

        packet->video_header.is_first_packet_in_frame = true;
        packet->video_header.is_last_packet_in_frame = true;
        return packet;
    }
};

class VCM_PacketBufferH264IdrIsKeyframeTest
    : public PacketBufferH264XIsKeyframeTest {
protected:
    VCM_PacketBufferH264IdrIsKeyframeTest()
        : PacketBufferH264XIsKeyframeTest(false) {}
};

TEST_F(VCM_PacketBufferH264IdrIsKeyframeTest, IdrIsKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(1);
    h264_header.nalus[0].type = h264::NaluType::IDR;
    h264_header.has_idr = true;
    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_frames,
                ElementsAre(KeyFrame()));
}

TEST_F(VCM_PacketBufferH264IdrIsKeyframeTest, SpsPpsIdrIsKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(3);
    h264_header.nalus[0].type = h264::NaluType::SPS;
    h264_header.nalus[1].type = h264::NaluType::PPS;
    h264_header.nalus[2].type = h264::NaluType::IDR;
    h264_header.has_sps = true;
    h264_header.has_pps = true;
    h264_header.has_idr = true;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_frames,
                ElementsAre(KeyFrame()));
}

class VCM_PacketBufferH264SpsPpsIdrIsKeyframeTest
    : public PacketBufferH264XIsKeyframeTest {
 protected:
  VCM_PacketBufferH264SpsPpsIdrIsKeyframeTest()
      : PacketBufferH264XIsKeyframeTest(true) {}
};

TEST_F(VCM_PacketBufferH264SpsPpsIdrIsKeyframeTest, IdrIsNotKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(1);
    h264_header.nalus[0].type = h264::NaluType::IDR;
    h264_header.has_sps = false;
    h264_header.has_pps = false;
    h264_header.has_idr = true;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_frames,
                ElementsAre(DeltaFrame()));
}

TEST_F(VCM_PacketBufferH264SpsPpsIdrIsKeyframeTest, SpsPpsIsNotKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(2);
    h264_header.nalus[0].type = h264::NaluType::SPS;
    h264_header.nalus[1].type = h264::NaluType::PPS;
    h264_header.has_sps = true;
    h264_header.has_pps = true;
    h264_header.has_idr = false;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_frames,
                ElementsAre(DeltaFrame()));
}

TEST_F(VCM_PacketBufferH264SpsPpsIdrIsKeyframeTest, SpsPpsIdrIsKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(3);
    h264_header.nalus[0].type = h264::NaluType::SPS;
    h264_header.nalus[1].type = h264::NaluType::PPS;
    h264_header.nalus[2].type = h264::NaluType::IDR;
    h264_header.has_sps = true;
    h264_header.has_pps = true;
    h264_header.has_idr = true;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_frames,
                ElementsAre(KeyFrame()));
}
    
} // namespace test
} // namespace naivertc
