#include "rtc/sdp/sdp_media_entry.hpp"
#include "rtc/sdp/sdp_media_entry_application.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"

#include <gtest/gtest.h>

#include <string>

namespace naivertc {
namespace test {

// Application entry
TEST(SDPEntryTest, CreateApplicationEntry) {
    sdp::Application app("data");

    EXPECT_EQ(app.mid(), "data");
    EXPECT_EQ(app.kind(), sdp::MediaEntry::Kind::APPLICATION);
}

// Video entry
TEST(SDPEntryTest, CreateVideoEntry) {
    sdp::Media video(sdp::MediaEntry::Kind::VIDEO, "video", "UDP/TLS/RTP/SAVPF", sdp::Direction::SEND_ONLY);

    EXPECT_EQ(video.mid(), "video");
    EXPECT_EQ(video.kind(), sdp::MediaEntry::Kind::VIDEO);
    EXPECT_EQ(video.direction(), sdp::Direction::SEND_ONLY);
}

// Audio entry
TEST(SDPEntryTest, CreateAudioEntry) {
    sdp::Media audio(sdp::MediaEntry::Kind::AUDIO, "audio", "UDP/TLS/RTP/SAVPF", sdp::Direction::RECV_ONLY);

    EXPECT_EQ(audio.mid(), "audio");
    EXPECT_EQ(audio.kind(), sdp::MediaEntry::Kind::AUDIO);
    EXPECT_EQ(audio.direction(), sdp::Direction::RECV_ONLY);
}

} // namespace test
} // namespace naivertc