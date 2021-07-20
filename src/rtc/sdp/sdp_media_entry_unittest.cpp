#include "rtc/sdp/sdp_media_entry.hpp"
#include "rtc/sdp/sdp_media_entry_application.hpp"
#include "rtc/sdp/sdp_media_entry_audio.hpp"
#include "rtc/sdp/sdp_media_entry_video.hpp"

#include <gtest/gtest.h>

#include <string>

namespace naivertc {
namespace test {

// Application entry
TEST(SDPEntryTest, CreateApplicationEntry) {
    sdp::Application app("data");

    EXPECT_EQ(app.mid(), "data");
    EXPECT_EQ(app.type(), sdp::MediaEntry::Type::APPLICATION);
}

// Video entry
TEST(SDPEntryTest, CreateVideoEntry) {
    sdp::Video video("video", sdp::Direction::SEND_ONLY);

    EXPECT_EQ(video.mid(), "video");
    EXPECT_EQ(video.type(), sdp::MediaEntry::Type::VIDEO);
    EXPECT_EQ(video.direction(), sdp::Direction::SEND_ONLY);
}

// Audio entry
TEST(SDPEntryTest, CreateAudioEntry) {
    sdp::Audio audio("audio", sdp::Direction::RECV_ONLY);

    EXPECT_EQ(audio.mid(), "audio");
    EXPECT_EQ(audio.type(), sdp::MediaEntry::Type::AUDIO);
    EXPECT_EQ(audio.direction(), sdp::Direction::RECV_ONLY);
}

} // namespace test
} // namespace naivertc