#include "rtc/media/media_track.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(MediaTrackConfigTest, CreateMediaTrackConfig) {
    MediaTrack::Configuration config(MediaTrack::Kind::VIDEO, "1");
    config.direction = MediaTrack::Direction::SEND_ONLY;
    config.rtx_enabled = true;
    config.AddFeedback(MediaTrack::RtcpFeedback::NACK);
    config.fec_codec = MediaTrack::FecCodec::ULP_FEC;
    config.cname = "video-stream";
    config.msid = "stream1";
    config.track_id = "video-track1";
   
    EXPECT_EQ(config.kind(), MediaTrack::Kind::VIDEO);
    EXPECT_EQ(config.mid(), "1");
    EXPECT_EQ(config.cname, "video-stream");
    EXPECT_EQ(config.msid, "stream1");
    EXPECT_EQ(config.track_id, "video-track1");
    EXPECT_EQ(config.rtcp_feedbacks().size(), 1);
    EXPECT_EQ(config.rtx_enabled, true);
    EXPECT_EQ(config.fec_codec, MediaTrack::FecCodec::ULP_FEC);
    
}

TEST(MediaTrackConfigTest, CreateVideoMediaSDPWithUlpFEC) {
    MediaTrack::Configuration config(MediaTrack::Kind::VIDEO, "1");
    config.direction = MediaTrack::Direction::SEND_ONLY;
    config.rtx_enabled = true;
    config.fec_codec = MediaTrack::FecCodec::ULP_FEC;
    config.cname = "video-stream";
    config.msid = "stream1";
    config.track_id = "video-track1";
    config.AddFeedback(MediaTrack::RtcpFeedback::NACK);
    EXPECT_TRUE(config.AddCodec(MediaTrack::Codec::H264));

    auto media = MediaTrack::BuildDescription(config);
    EXPECT_TRUE(media.has_value());

    EXPECT_EQ(media->direction(), sdp::Direction::SEND_ONLY);
    // Payload type: h264(+ rtx) + red(+ rtx) + fec
    EXPECT_EQ(media->payload_types().size(), 5);
    EXPECT_EQ(media->media_ssrcs().size(), 1);
    EXPECT_EQ(media->rtx_ssrcs().size(), 1);
    EXPECT_EQ(media->fec_ssrcs().size(), 0);
}

TEST(MediaTrackConfigTest, CreateVideoMediaSDPWithFlexFEC) {
    MediaTrack::Configuration config(MediaTrack::Kind::VIDEO, "1");
    config.direction = MediaTrack::Direction::SEND_ONLY;
    config.rtx_enabled = true;
    config.fec_codec = MediaTrack::FecCodec::FLEX_FEC;
    config.AddFeedback(MediaTrack::RtcpFeedback::NACK);
    EXPECT_TRUE(config.AddCodec(MediaTrack::Codec::H264));

    auto media = MediaTrack::BuildDescription(config);
    EXPECT_TRUE(media.has_value());

    EXPECT_EQ(media->direction(), sdp::Direction::SEND_ONLY);
    // Payload type: h264(+ rtx) + fec
    EXPECT_EQ(media->payload_types().size(), 3);
    EXPECT_EQ(media->media_ssrcs().size(), 1);
    EXPECT_EQ(media->rtx_ssrcs().size(), 1);
    EXPECT_EQ(media->fec_ssrcs().size(), 1);
}

TEST(MediaTrackConfigTest, CreateAudioMediaSDP) {
    MediaTrack::Configuration config(MediaTrack::Kind::AUDIO, "1");
    config.direction = MediaTrack::Direction::SEND_ONLY;
    config.cname = "audio-stream";
    config.msid = "stream1";
    config.track_id = "audio-track1";
    config.rtx_enabled = false;
    EXPECT_TRUE(config.AddCodec(MediaTrack::Codec::OPUS));

    auto media = MediaTrack::BuildDescription(config);
    EXPECT_TRUE(media.has_value());

    EXPECT_EQ(media->direction(), sdp::Direction::SEND_ONLY);
    // Payload type: opus
    EXPECT_EQ(media->payload_types().size(), 1);
    EXPECT_EQ(media->media_ssrcs().size(), 1);
    EXPECT_EQ(media->rtx_ssrcs().size(), 0);
    EXPECT_EQ(media->fec_ssrcs().size(), 0);
}

} // namespace test
} // namespace naivertc