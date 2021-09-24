#include "rtc/media/media_track.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(MediaTrackConfigTest, CreateConfig) {
    MediaTrack::Configuration config("1", MediaTrack::Kind::VIDEO, MediaTrack::Codec::H264);
    config.nack_enabled = true;
    config.rtx_enabled = true;
    config.fec_codec.emplace(MediaTrack::FecCodec::ULP_FEC);
    config.cname.emplace("video-stream");
    config.msid.emplace("stream1");
    config.track_id.emplace("video-track1");

    EXPECT_EQ(config.mid, "1");
    EXPECT_EQ(config.kind, MediaTrack::Kind::VIDEO);
    EXPECT_EQ(config.codec, MediaTrack::Codec::H264);
    EXPECT_EQ(config.nack_enabled, true);
    EXPECT_EQ(config.rtx_enabled, true);
    EXPECT_EQ(config.fec_codec, MediaTrack::FecCodec::ULP_FEC);
    EXPECT_EQ(config.cname, "video-stream");
    EXPECT_EQ(config.msid, "stream1");
    EXPECT_EQ(config.track_id, "video-track1");

}

TEST(MediaTrackConfigTest, BuildVideoDescription) {

    MediaTrack::Configuration config("1", MediaTrack::Kind::VIDEO, MediaTrack::Codec::H264);
    config.nack_enabled = true;
    config.rtx_enabled = true;
    config.fec_codec.emplace(MediaTrack::FecCodec::ULP_FEC);
    config.cname.emplace("video-stream");
    config.msid.emplace("stream1");
    config.track_id.emplace("video-track1");

    auto media = MediaTrack::BuildDescription(config);
    EXPECT_TRUE(media.has_value());
    // Media + RTX
    EXPECT_EQ(media->media_ssrcs().size(), 1);
    EXPECT_EQ(media->rtx_ssrcs().size(), 1);
}

TEST(MediaTrackConfigTest, BuildAudioDescription) {

    MediaTrack::Configuration config("1", MediaTrack::Kind::AUDIO, MediaTrack::Codec::OPUS);
    config.cname.emplace("audio-stream");
    config.msid.emplace("stream1");
    config.track_id.emplace("audio-track1");

    auto media = MediaTrack::BuildDescription(config);
    EXPECT_TRUE(media.has_value());
    // Media
    EXPECT_EQ(media->media_ssrcs().size(), 1);
}

}
}