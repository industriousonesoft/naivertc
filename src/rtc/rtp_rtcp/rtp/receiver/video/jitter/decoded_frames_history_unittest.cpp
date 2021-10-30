#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtc::video;

namespace naivertc {
namespace test {

constexpr int kHistorySize = 1 << 13; // 8192;

TEST(VCM_DecodedFramesHistoryTest, RequestOnEmptyHistory) {
    DecodedFramesHistory history(kHistorySize);
    EXPECT_EQ(history.WasDecoded(1234), false);
}

TEST(VCM_DecodedFramesHistoryTest, FindsLastDecodedFrame) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(1234, 0);
    EXPECT_EQ(history.WasDecoded(1234), true);
}

TEST(VCM_DecodedFramesHistoryTest, FindsPreviousFrame) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(1234, 0);
    history.InsertFrame(1235, 0);
    EXPECT_EQ(history.WasDecoded(1234), true);
}

TEST(VCM_DecodedFramesHistoryTest, ReportsMissingFrame) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(1234, 0);
    history.InsertFrame(1236, 0);
    EXPECT_EQ(history.WasDecoded(1235), false);
}

TEST(VCM_DecodedFramesHistoryTest, ClearsHistory) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(1234, 0);
    history.Clear();
    EXPECT_EQ(history.WasDecoded(1234), false);
    EXPECT_EQ(history.last_decoded_frame_id(), std::nullopt);
    EXPECT_EQ(history.last_decoded_frame_timestamp(), std::nullopt);
}

TEST(VCM_DecodedFramesHistoryTest, HandlesBigJumpInPictureId) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(1234, 0);
    history.InsertFrame(1235, 0);
    history.InsertFrame(1236, 0);
    history.InsertFrame(1236 + kHistorySize / 2, 0);
    EXPECT_EQ(history.WasDecoded(1234), true);
    EXPECT_EQ(history.WasDecoded(1237), false);
}

TEST(VCM_DecodedFramesHistoryTest, ForgetsTooOldHistory) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(1234, 0);
    history.InsertFrame(1235, 0);
    history.InsertFrame(1236, 0);
    history.InsertFrame(1236 + kHistorySize * 2, 0);
    EXPECT_EQ(history.WasDecoded(1234), false);
    EXPECT_EQ(history.WasDecoded(1237), false);
}

TEST(VCM_DecodedFramesHistoryTest, ReturnsLastDecodedFrameId) {
    DecodedFramesHistory history(kHistorySize);
    EXPECT_EQ(history.last_decoded_frame_id(), std::nullopt);
    history.InsertFrame(1234, 0);
    EXPECT_EQ(history.last_decoded_frame_id(), 1234);
    history.InsertFrame(1235, 0);
    EXPECT_EQ(history.last_decoded_frame_id(), 1235);
}

TEST(VCM_DecodedFramesHistoryTest, ReturnsLastDecodedFrameTimestamp) {
    DecodedFramesHistory history(kHistorySize);
    EXPECT_EQ(history.last_decoded_frame_timestamp(), std::nullopt);
    history.InsertFrame(1234, 12345);
    EXPECT_EQ(history.last_decoded_frame_timestamp(), 12345u);
    history.InsertFrame(1235, 12366);
    EXPECT_EQ(history.last_decoded_frame_timestamp(), 12366u);
}

TEST(VCM_DecodedFramesHistoryTest, NegativePictureIds) {
    DecodedFramesHistory history(kHistorySize);
    history.InsertFrame(-1234, 12345);
    history.InsertFrame(-1233, 12366);
    EXPECT_EQ(*history.last_decoded_frame_id(), -1233);

    history.InsertFrame(-1, 12377);
    history.InsertFrame(0, 12388);
    EXPECT_EQ(*history.last_decoded_frame_id(), 0);

    history.InsertFrame(1, 12399);
    EXPECT_EQ(*history.last_decoded_frame_id(), 1);

    EXPECT_EQ(history.WasDecoded(-1234), true);
    EXPECT_EQ(history.WasDecoded(-1), true);
    EXPECT_EQ(history.WasDecoded(0), true);
    EXPECT_EQ(history.WasDecoded(1), true);
}
    
} // namespace test
} // namespace naivertc