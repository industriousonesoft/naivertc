#include "pc/sdp/sdp_entry.hpp"

#include <gtest/gtest.h>

#include <string>

namespace naivertc {
namespace test {

// Application entry
TEST(SDPEntryTest, CreateApplicationEntry) {
    sdp::Application app("data");

    EXPECT_EQ(app.mid(), "data");
    EXPECT_EQ(app.type(), sdp::Entry::Type::APPLICATION);
    EXPECT_EQ(app.direction(), sdp::Direction::SEND_RECV);
}

// Video entry
TEST(SDPEntryTest, CreateVideoEntry) {
    sdp::Video video("video", sdp::Direction::SEND_ONLY);

    EXPECT_EQ(video.mid(), "video");
    EXPECT_EQ(video.type(), sdp::Entry::Type::VIDEO);
    EXPECT_EQ(video.direction(), sdp::Direction::SEND_ONLY);
}

// Audio entry
TEST(SDPEntryTest, CreateAudioEntry) {
    sdp::Audio audio("audio", sdp::Direction::RECV_ONLY);

    EXPECT_EQ(audio.mid(), "audio");
    EXPECT_EQ(audio.type(), sdp::Entry::Type::AUDIO);
    EXPECT_EQ(audio.direction(), sdp::Direction::RECV_ONLY);
}

}
}