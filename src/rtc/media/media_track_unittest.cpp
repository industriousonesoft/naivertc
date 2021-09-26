#include "rtc/media/media_track.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(MediaTrackConfigTest, CreateConfigAndCodecParams) {
    MediaTrack::Configuration config(MediaTrack::Kind::VIDEO, "1", "UDP/TLS/RTP/SAVPF", "video-stream", "stream1", "video-track1");
    MediaTrack::CodecParams cp(MediaTrack::Codec::H264, true, true, MediaTrack::FecCodec::ULP_FEC);

    EXPECT_EQ(config.kind, MediaTrack::Kind::VIDEO);
    EXPECT_EQ(config.mid, "1");
    EXPECT_EQ(config.transport_protocols, "UDP/TLS/RTP/SAVPF");
    EXPECT_EQ(config.cname, "video-stream");
    EXPECT_EQ(config.msid, "stream1");
    EXPECT_EQ(config.track_id, "video-track1");

    EXPECT_EQ(cp.codec, MediaTrack::Codec::H264);
    EXPECT_EQ(cp.nack_enabled, true);
    EXPECT_EQ(cp.rtx_enabled, true);
    EXPECT_EQ(cp.fec_codec, MediaTrack::FecCodec::ULP_FEC);
    

}

TEST(MediaTrackConfigTest, CreateVideoMediaTrack) {
    MediaTrack::Configuration config(MediaTrack::Kind::VIDEO, "1");
    config.cname.emplace("video-stream");
    config.msid.emplace("stream1");
    config.track_id.emplace("video-track1");

    auto media = MediaTrack::CreateDescription(config);
    EXPECT_TRUE(media.has_value());
    EXPECT_EQ(media->media_ssrcs().size(), 0);
    EXPECT_EQ(media->rtx_ssrcs().size(), 0);
    EXPECT_EQ(media->fec_ssrcs().size(), 0);

    MediaTrack media_track(media.value());

    // Codec: Media + RTX + FEC
    MediaTrack::CodecParams cp(MediaTrack::Codec::H264, true, true, MediaTrack::FecCodec::ULP_FEC);
    EXPECT_TRUE(media_track.AddCodec(cp));
    // Payload type: h264 + nack + rtx + red + fec
    EXPECT_EQ(media_track.description()->payload_types().size(), 5);
    // EXPECT_EQ(media_track.description()->media_ssrcs().size(), 1);
    // EXPECT_EQ(media_track.description()->rtx_ssrcs().size(), 1);
    // EXPECT_EQ(media_track.description()->fec_ssrcs().size(), 0);
}

TEST(MediaTrackConfigTest, CreateAudioMediaTrack) {

    MediaTrack::Configuration config(MediaTrack::Kind::AUDIO, "1");
    config.cname.emplace("audio-stream");
    config.msid.emplace("stream1");
    config.track_id.emplace("audio-track1");

    auto media = MediaTrack::CreateDescription(config);
    EXPECT_TRUE(media.has_value());
    EXPECT_EQ(media->media_ssrcs().size(), 0);
    EXPECT_EQ(media->rtx_ssrcs().size(), 0);
    EXPECT_EQ(media->fec_ssrcs().size(), 0);

    MediaTrack media_track(media.value());
    // Codec: Media + RTX
    MediaTrack::CodecParams cp(MediaTrack::Codec::OPUS, false, false);
    EXPECT_TRUE(media_track.AddCodec(cp));
    EXPECT_EQ(media_track.description()->payload_types().size(), 1);
    // EXPECT_EQ(media_track.description()->media_ssrcs().size(), 1);
    // EXPECT_EQ(media_track.description()->rtx_ssrcs().size(), 1);
    // EXPECT_EQ(media_track.description()->fec_ssrcs().size(), 0);
}

} // namespace test
} // namespace naivertc