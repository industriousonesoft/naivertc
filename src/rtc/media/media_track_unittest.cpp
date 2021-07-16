#include "rtc/media/media_track.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(MediaTrackConfigTest, CreateConfig) {
    MediaTrack::Config config("1", MediaTrack::Kind::VIDEO, MediaTrack::Codec::H264, {102}, 1, "video-stream", "stream1", "video-track1");

    EXPECT_EQ(config.mid, "1");
    EXPECT_EQ(config.kind, MediaTrack::Kind::VIDEO);
    EXPECT_EQ(config.codec, MediaTrack::Codec::H264);
    EXPECT_EQ(config.payload_types[0], 102);
    EXPECT_EQ(config.ssrc, 1);
    EXPECT_EQ(config.cname, "video-stream");
    EXPECT_EQ(config.msid, "stream1");
    EXPECT_EQ(config.track_id, "video-track1");

}

}
}