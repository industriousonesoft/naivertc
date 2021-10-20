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

using Packet = video::jitter::PacketBuffer::Packet;
using InsertResult = video::jitter::PacketBuffer::InsertResult;

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

class Video_Jitter_PacketBufferTest : public ::testing::Test {
protected:
    Video_Jitter_PacketBufferTest() : packet_buffer_(kStartSize, kMaxSize) {}

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
        packet->video_header.codec_type = video::CodecType::GENERIC;
        packet->timestamp = timestamp;
        packet->seq_num = seq_num;
        packet->video_header.frame_type = keyframe == kKeyFrame
                                            ? video::FrameType::KEY
                                            : video::FrameType::DELTA;
        packet->video_header.is_first_packet_in_frame = first == kFirst;
        packet->video_header.is_last_packet_in_frame = last == kLast;
        packet->video_payload.Assign(data.data(), data.size());

        return packet_buffer_.InsertPacket(std::move(packet));
    }

    video::jitter::PacketBuffer packet_buffer_;
};

TEST_F(Video_Jitter_PacketBufferTest, InsertOnePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
}

TEST_F(Video_Jitter_PacketBufferTest, InsertMultiplePackets) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(seq_num + 3, kKeyFrame, kFirst, kLast).assembled_packets, SizeIs(1));
}

TEST_F(Video_Jitter_PacketBufferTest, InsertDuplicatePacket) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast).assembled_packets, SizeIs(2));
}

TEST_F(Video_Jitter_PacketBufferTest, SeqNumWrapOneFrame) {
    Insert(0xFFFF, kKeyFrame, kFirst, kNotLast);
    auto ret = Insert(0x00, kKeyFrame, kNotFirst, kLast);
    EXPECT_THAT(ret.assembled_packets, SizeIs(2));
    EXPECT_THAT(ret, StartSeqNumsAre(0xFFFF));
}

TEST_F(Video_Jitter_PacketBufferTest, SeqNumWrapTwoFrames) {
    EXPECT_THAT(Insert(0xFFFF, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0xFFFF));
    EXPECT_THAT(Insert(0x0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0x0));
}

TEST_F(Video_Jitter_PacketBufferTest, InsertOldPackets) {
    EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_EQ(Insert(102, kDeltaFrame, kFirst, kLast).assembled_packets.size(), 1u);
    EXPECT_EQ(Insert(101, kKeyFrame, kNotFirst, kLast).assembled_packets.size(), 2u);

    EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));

    packet_buffer_.ClearTo(102);
    EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(103, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));
}

TEST_F(Video_Jitter_PacketBufferTest, FrameSize) {
    const uint16_t seq_num = Rand();
    uint8_t data1[5] = {};
    uint8_t data2[5] = {};
    uint8_t data3[5] = {};
    uint8_t data4[5] = {};

    Insert(seq_num, kKeyFrame, kFirst, kNotLast, data1);
    Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, data2);
    Insert(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, data3);
    auto packets = Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, data4).assembled_packets;
    // Expect one frame of 4 packets.
    EXPECT_THAT(StartSeqNums(packets), ElementsAre(seq_num));
    EXPECT_THAT(packets, SizeIs(4));
}

TEST_F(Video_Jitter_PacketBufferTest, ExpandBuffer) {
    const uint16_t seq_num = Rand();

    Insert(seq_num, kKeyFrame, kFirst, kNotLast);
    for (int i = 1; i < kStartSize; ++i)
        EXPECT_FALSE(Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast).keyframe_requested);

    // Already inserted kStartSize number of packets, inserting the last packet
    // should increase the buffer size and also result in an assembled frame.
    EXPECT_FALSE(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast).keyframe_requested);
}

TEST_F(Video_Jitter_PacketBufferTest, SingleFrameExpandsBuffer) {
    const uint16_t seq_num = Rand();

    Insert(seq_num, kKeyFrame, kFirst, kNotLast);
    for (int i = 1; i < kStartSize; ++i)
        Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast), StartSeqNumsAre(seq_num));
}

TEST_F(Video_Jitter_PacketBufferTest, ExpandBufferOverflow) {
    const uint16_t seq_num = Rand();

    EXPECT_FALSE(Insert(seq_num, kKeyFrame, kFirst, kNotLast).keyframe_requested);
    for (int i = 1; i < kMaxSize; ++i)
        EXPECT_FALSE(Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast).keyframe_requested);

    // Already inserted kMaxSize number of packets, inserting the last packet
    // should overflow the buffer and result in false being returned.
    EXPECT_TRUE(Insert(seq_num + kMaxSize, kKeyFrame, kNotFirst, kLast).keyframe_requested);
}


TEST_F(Video_Jitter_PacketBufferTest, OnePacketOneFrame) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast), StartSeqNumsAre(seq_num));
}

TEST_F(Video_Jitter_PacketBufferTest, TwoPacketsTwoFrames) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast), StartSeqNumsAre(seq_num));
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast), StartSeqNumsAre(seq_num + 1));
}

TEST_F(Video_Jitter_PacketBufferTest, TwoPacketsOneFrames) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast), StartSeqNumsAre(seq_num));
}

TEST_F(Video_Jitter_PacketBufferTest, ThreePacketReorderingOneFrame) {
    const uint16_t seq_num = Rand();
    EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kNotFirst, kLast).assembled_packets, IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast), StartSeqNumsAre(seq_num));
}

TEST_F(Video_Jitter_PacketBufferTest, Frames) {
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

TEST_F(Video_Jitter_PacketBufferTest, ClearSinglePacket) {
    const uint16_t seq_num = Rand();

    for (int i = 0; i < kMaxSize; ++i)
        Insert(seq_num + i, kDeltaFrame, kFirst, kLast);

    packet_buffer_.ClearTo(seq_num);
    EXPECT_FALSE(Insert(seq_num + kMaxSize, kDeltaFrame, kFirst, kLast).keyframe_requested);
}

TEST_F(Video_Jitter_PacketBufferTest, ClearFullBuffer) {
    for (int i = 0; i < kMaxSize; ++i)
        Insert(i, kDeltaFrame, kFirst, kLast);

    packet_buffer_.ClearTo(kMaxSize - 1);

    for (int i = kMaxSize; i < 2 * kMaxSize; ++i)
        EXPECT_FALSE(Insert(i, kDeltaFrame, kFirst, kLast).keyframe_requested);
}

TEST_F(Video_Jitter_PacketBufferTest, DontClearNewerPacket) {
    EXPECT_THAT(Insert(0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0));
    packet_buffer_.ClearTo(0);
    EXPECT_THAT(Insert(2 * kStartSize, kKeyFrame, kFirst, kLast),
                StartSeqNumsAre(2 * kStartSize));
    EXPECT_THAT(Insert(3 * kStartSize + 1, kKeyFrame, kFirst, kNotLast).assembled_packets,
                IsEmpty());
    packet_buffer_.ClearTo(2 * kStartSize);
    EXPECT_THAT(Insert(3 * kStartSize + 2, kKeyFrame, kNotFirst, kLast),
                StartSeqNumsAre(3 * kStartSize + 1));
}

TEST_F(Video_Jitter_PacketBufferTest, OneIncompleteFrame) {
    const uint16_t seq_num = Rand();

    EXPECT_THAT(Insert(seq_num, kDeltaFrame, kFirst, kNotLast).assembled_packets,
                IsEmpty());
    EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kLast),
                StartSeqNumsAre(seq_num));
    EXPECT_THAT(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast).assembled_packets,
                IsEmpty());
}

TEST_F(Video_Jitter_PacketBufferTest, TwoIncompleteFramesFullBuffer) {
    const uint16_t seq_num = Rand();

    for (int i = 1; i < kMaxSize - 1; ++i)
        Insert(seq_num + i, kDeltaFrame, kNotFirst, kNotLast);
    EXPECT_THAT(Insert(seq_num, kDeltaFrame, kFirst, kNotLast).assembled_packets,
                IsEmpty());
    EXPECT_THAT(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast).assembled_packets,
                IsEmpty());
}

TEST_F(Video_Jitter_PacketBufferTest, FramesReordered) {
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

TEST_F(Video_Jitter_PacketBufferTest, InsertPacketAfterSequenceNumberWrapAround) {
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
    auto packets = Insert(seq_num++, kKeyFrame, kNotFirst, kLast, {}, timestamp).assembled_packets;
    // One frame of 7 packets.
    EXPECT_THAT(StartSeqNums(packets), SizeIs(1));
    EXPECT_THAT(packets, SizeIs(7));
}

TEST_F(Video_Jitter_PacketBufferTest, FreeSlotsOnFrameCreation) {
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

TEST_F(Video_Jitter_PacketBufferTest, Clear) {
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

TEST_F(Video_Jitter_PacketBufferTest, FramesAfterClear) {
    Insert(9025, kDeltaFrame, kFirst, kLast);
    Insert(9024, kKeyFrame, kFirst, kLast);
    packet_buffer_.ClearTo(9025);
    EXPECT_THAT(Insert(9057, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));
    EXPECT_THAT(Insert(9026, kDeltaFrame, kFirst, kLast).assembled_packets, SizeIs(1));
}

TEST_F(Video_Jitter_PacketBufferTest, SameFrameDifferentTimestamps) {
    Insert(0, kKeyFrame, kFirst, kNotLast, {}, 1000);
    EXPECT_THAT(Insert(1, kKeyFrame, kNotFirst, kLast, {}, 1001).assembled_packets,
                IsEmpty());
}

TEST_F(Video_Jitter_PacketBufferTest, ContinuousSeqNumDoubleMarkerBit) {
    Insert(2, kKeyFrame, kNotFirst, kNotLast);
    Insert(1, kKeyFrame, kFirst, kLast);
    EXPECT_THAT(Insert(3, kKeyFrame, kNotFirst, kLast).assembled_packets, IsEmpty());
}

TEST_F(Video_Jitter_PacketBufferTest, TooManyNalusInPacket) {
    auto packet = std::make_unique<Packet>();
    packet->video_header.codec_type = video::CodecType::H264;
    packet->timestamp = 1;
    packet->seq_num = 1;
    packet->video_header.frame_type = video::FrameType::KEY;
    packet->video_header.is_first_packet_in_frame = true;
    packet->video_header.is_last_packet_in_frame = true;
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(h264::kMaxNaluNumPerPacket);
    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_packets,
                IsEmpty());
}

// If |sps_pps_idr_is_keyframe| is true, we require keyframes to contain
// SPS/PPS/IDR and the keyframes we create as part of the test do contain
// SPS/PPS/IDR. If |sps_pps_idr_is_keyframe| is false, we only require and
// create keyframes containing only IDR.
class PacketBufferH264Test : public Video_Jitter_PacketBufferTest {
protected:
    explicit PacketBufferH264Test(bool sps_pps_idr_is_keyframe)
        : Video_Jitter_PacketBufferTest() {
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
        packet->video_header.codec_type = video::CodecType::H264;
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
class Video_Jitter_PacketBufferH264ParameterizedTest
    : public ::testing::WithParamInterface<bool>,
      public PacketBufferH264Test {
protected:
    Video_Jitter_PacketBufferH264ParameterizedTest() : PacketBufferH264Test(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(SpsPpsIdrIsKeyframe,
                         Video_Jitter_PacketBufferH264ParameterizedTest,
                         ::testing::Bool());

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, DontRemoveMissingPacketOnClearTo) {
    InsertH264(0, kKeyFrame, kFirst, kLast, 0);
    InsertH264(2, kDeltaFrame, kFirst, kNotLast, 2);
    packet_buffer_.ClearTo(0);
    // Expect no frame because of missing of packet #1
    EXPECT_THAT(InsertH264(3, kDeltaFrame, kNotFirst, kLast, 2).assembled_packets,
                IsEmpty());
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, GetBitstreamOneFrameFullBuffer) {
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

    auto packets = InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1, data_arr[kStartSize - 1]).assembled_packets;
    ASSERT_THAT(StartSeqNums(packets), ElementsAre(0));
    EXPECT_THAT(packets, SizeIs(kStartSize));
    for (size_t i = 0; i < packets.size(); ++i) {
        EXPECT_THAT(packets[i]->video_payload, SizeIs(1)) << "Packet #" << i;
    }
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, GetBitstreamBufferPadding) {
    uint16_t seq_num = Rand();
    CopyOnWriteBuffer data = "some plain old data";

    auto packet = std::make_unique<Packet>();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
     h264_header.nalus.resize(1);
    h264_header.nalus[0].type = h264::NaluType::IDR;
    h264_header.packetization_type = h264::PacketizationType::SIGNLE;
    packet->seq_num = seq_num;
    packet->video_header.codec_type = video::CodecType::H264;
    packet->video_payload = data;
    packet->video_header.is_first_packet_in_frame = true;
    packet->video_header.is_last_packet_in_frame = true;
    auto frames = packet_buffer_.InsertPacket(std::move(packet)).assembled_packets;

    ASSERT_THAT(frames, SizeIs(1));
    EXPECT_EQ(frames[0]->seq_num, seq_num);
    EXPECT_EQ(frames[0]->video_payload, data);
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, FrameResolution) {
    uint16_t seq_num = 100;
    uint8_t data[] = "some plain old data";
    uint32_t width = 640;
    uint32_t height = 360;
    uint32_t timestamp = 1000;

    auto packets = InsertH264(seq_num, kKeyFrame, kFirst, kLast, timestamp, data, width, height).assembled_packets;

    ASSERT_THAT(packets, SizeIs(1));
    EXPECT_EQ(packets[0]->video_header.frame_width, width);
    EXPECT_EQ(packets[0]->video_header.frame_height, height);
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, OneFrameFillBuffer) {
    InsertH264(0, kKeyFrame, kFirst, kNotLast, 1000);
    for (int i = 1; i < kStartSize - 1; ++i)
        InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1000);
    EXPECT_THAT(InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1000),
                StartSeqNumsAre(0));
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, CreateFramesAfterFilledBuffer) {
    EXPECT_THAT(InsertH264(kStartSize - 2, kKeyFrame, kFirst, kLast, 0).assembled_packets,
                SizeIs(1));

    InsertH264(kStartSize, kDeltaFrame, kFirst, kNotLast, 2000);
    for (int i = 1; i < kStartSize; ++i)
        InsertH264(kStartSize + i, kDeltaFrame, kNotFirst, kNotLast, 2000);

    EXPECT_THAT(InsertH264(kStartSize + kStartSize, kDeltaFrame, kNotFirst, kLast, 2000).assembled_packets,
                IsEmpty());

    EXPECT_THAT(InsertH264(kStartSize - 1, kKeyFrame, kFirst, kLast, 1000),
                StartSeqNumsAre(kStartSize - 1, kStartSize));
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, OneFrameMaxSeqNum) {
    InsertH264(65534, kKeyFrame, kFirst, kNotLast, 1000);
    EXPECT_THAT(InsertH264(65535, kKeyFrame, kNotFirst, kLast, 1000),
                StartSeqNumsAre(65534));
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, ClearMissingPacketsOnKeyframe) {
    EXPECT_THAT(InsertH264(0, kKeyFrame, kFirst, kLast, 1000), StartSeqNumsAre(0));
    EXPECT_THAT(InsertH264(2, kKeyFrame, kFirst, kLast, 3000).assembled_packets, SizeIs(1));
    EXPECT_THAT(InsertH264(3, kDeltaFrame, kFirst, kNotLast, 4000).assembled_packets, SizeIs(0));
    EXPECT_THAT(InsertH264(4, kDeltaFrame, kNotFirst, kLast, 4000).assembled_packets, SizeIs(2));

    auto ret = InsertH264(10, kKeyFrame, kFirst, kLast, 18000);
    EXPECT_EQ(ret.assembled_packets.size(), 1);
    EXPECT_THAT(InsertH264(kStartSize + 1, kKeyFrame, kFirst, kLast, 18000),
                StartSeqNumsAre(kStartSize + 1));
}

TEST_P(Video_Jitter_PacketBufferH264ParameterizedTest, FindFramesOnPadding) {
    EXPECT_THAT(InsertH264(0, kKeyFrame, kFirst, kLast, 1000), StartSeqNumsAre(0));
    EXPECT_THAT(InsertH264(2, kDeltaFrame, kFirst, kLast, 1000).assembled_packets,
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
        packet->video_header.codec_type = video::CodecType::H264;
        packet->seq_num = kSeqNum;

        packet->video_header.is_first_packet_in_frame = true;
        packet->video_header.is_last_packet_in_frame = true;
        return packet;
    }
};

class Video_Jitter_PacketBufferH264IdrIsKeyframeTest
    : public PacketBufferH264XIsKeyframeTest {
protected:
    Video_Jitter_PacketBufferH264IdrIsKeyframeTest()
        : PacketBufferH264XIsKeyframeTest(false) {}
};

TEST_F(Video_Jitter_PacketBufferH264IdrIsKeyframeTest, IdrIsKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(1);
    h264_header.nalus[0].type = h264::NaluType::IDR;
    h264_header.has_idr = true;
    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_packets,
                ElementsAre(KeyFrame()));
}

TEST_F(Video_Jitter_PacketBufferH264IdrIsKeyframeTest, SpsPpsIdrIsKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(3);
    h264_header.nalus[0].type = h264::NaluType::SPS;
    h264_header.nalus[1].type = h264::NaluType::PPS;
    h264_header.nalus[2].type = h264::NaluType::IDR;
    h264_header.has_sps = true;
    h264_header.has_pps = true;
    h264_header.has_idr = true;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_packets,
                ElementsAre(KeyFrame()));
}

class Video_Jitter_PacketBufferH264SpsPpsIdrIsKeyframeTest
    : public PacketBufferH264XIsKeyframeTest {
 protected:
  Video_Jitter_PacketBufferH264SpsPpsIdrIsKeyframeTest()
      : PacketBufferH264XIsKeyframeTest(true) {}
};

TEST_F(Video_Jitter_PacketBufferH264SpsPpsIdrIsKeyframeTest, IdrIsNotKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(1);
    h264_header.nalus[0].type = h264::NaluType::IDR;
    h264_header.has_sps = false;
    h264_header.has_pps = false;
    h264_header.has_idr = true;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_packets,
                ElementsAre(DeltaFrame()));
}

TEST_F(Video_Jitter_PacketBufferH264SpsPpsIdrIsKeyframeTest, SpsPpsIsNotKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(2);
    h264_header.nalus[0].type = h264::NaluType::SPS;
    h264_header.nalus[1].type = h264::NaluType::PPS;
    h264_header.has_sps = true;
    h264_header.has_pps = true;
    h264_header.has_idr = false;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_packets,
                ElementsAre(DeltaFrame()));
}

TEST_F(Video_Jitter_PacketBufferH264SpsPpsIdrIsKeyframeTest, SpsPpsIdrIsKeyframe) {
    auto packet = CreatePacket();
    auto& h264_header = packet->video_codec_header.emplace<h264::PacketizationInfo>();
    h264_header.nalus.resize(3);
    h264_header.nalus[0].type = h264::NaluType::SPS;
    h264_header.nalus[1].type = h264::NaluType::PPS;
    h264_header.nalus[2].type = h264::NaluType::IDR;
    h264_header.has_sps = true;
    h264_header.has_pps = true;
    h264_header.has_idr = true;

    EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).assembled_packets,
                ElementsAre(KeyFrame()));
}
    
} // namespace test
} // namespace naivertc
