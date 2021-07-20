#include "rtc/sdp/sdp_description.hpp"

#include <gtest/gtest.h>

#include <sstream>

namespace naivertc {
namespace test {

TEST(DescriptionTest, BuildAnOffer) {
    auto builder = sdp::Description::Builder(sdp::Type::OFFER)
                    .set_role(sdp::Role::ACT_PASS)
                    .set_ice_ufrag("KTqE")
                    .set_ice_pwd("u8XPW6fYzsDGjQmCYCQ+9W8S")
                    .set_fingerprint("8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    auto local_sdp = builder.Build();
    local_sdp.AddApplication("0");
    local_sdp.AddAudio("1", sdp::Direction::SEND_RECV);
    local_sdp.AddVideo("2", sdp::Direction::SEND_RECV);

    EXPECT_EQ(local_sdp.type(), sdp::Type::OFFER);
    EXPECT_EQ(local_sdp.role(), sdp::Role::ACT_PASS);
    EXPECT_EQ(local_sdp.ice_ufrag().has_value(), true);
    EXPECT_EQ(local_sdp.ice_ufrag().value(), "KTqE");
    EXPECT_EQ(local_sdp.ice_pwd().has_value(), true);
    EXPECT_EQ(local_sdp.ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_EQ(local_sdp.fingerprint().has_value(), true);
    EXPECT_EQ(local_sdp.fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(local_sdp.HasApplication(), true);
    EXPECT_EQ(local_sdp.HasAudio(), true);
    EXPECT_EQ(local_sdp.HasVideo(), true);
}

TEST(DescriptionTest, ParseAnOffer) {
    const std::string remote_sdp_string = R"(v=0
    o=- 9054970245222891759 2 IN IP4 127.0.0.1
    s=-
    t=0 0
    a=group:BUNDLE 0 2 1
    a=msid-semantic: WMS
    m=application 9 UDP/DTLS/SCTP webrtc-datachannel
    c=IN IP4 0.0.0.0
    a=ice-ufrag:KTqE
    a=ice-pwd:u8XPW6fYzsDGjQmCYCQ+9W8S
    a=ice-options:trickle
    a=fingerprint:sha-256 8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB
    a=setup:active
    a=mid:0
    a=sctp-port:5000
    a=max-message-size:262144
    m=audio 9 UDP/TLS/RTP/SAVPF 111
    c=IN IP4 0.0.0.0
    a=rtcp:9 IN IP4 0.0.0.0
    a=ice-ufrag:KTqE
    a=ice-pwd:u8XPW6fYzsDGjQmCYCQ+9W8S
    a=ice-options:trickle
    a=fingerprint:sha-256 8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB
    a=setup:active
    a=mid:1
    a=recvonly
    a=rtcp-mux
    a=rtpmap:111 opus/48000/2
    a=fmtp:111 minptime=10;useinbandfec=1
    m=video 9 UDP/TLS/RTP/SAVPF 102
    c=IN IP4 0.0.0.0
    a=rtcp:9 IN IP4 0.0.0.0
    a=ice-ufrag:KTqE
    a=ice-pwd:u8XPW6fYzsDGjQmCYCQ+9W8S
    a=ice-options:trickle
    a=fingerprint:sha-256 8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB
    a=setup:active
    a=mid:2
    a=recvonly
    a=rtcp-mux
    a=rtpmap:102 h264/90000
    a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f)";

    auto remote_sdp = sdp::Description::Parser::Parse(remote_sdp_string, sdp::Type::OFFER);
    auto app = remote_sdp.application();
    auto audio = remote_sdp.media("1");
    auto video = remote_sdp.media("2");

    EXPECT_EQ(remote_sdp.type(), sdp::Type::OFFER);
    EXPECT_EQ(remote_sdp.role(), sdp::Role::ACTIVE);
    EXPECT_EQ(remote_sdp.ice_ufrag().has_value(), false);
    EXPECT_EQ(remote_sdp.ice_pwd().has_value(), false);
    EXPECT_EQ(remote_sdp.fingerprint().has_value(), false);
    EXPECT_EQ(remote_sdp.HasApplication(), true);
    EXPECT_EQ(remote_sdp.HasAudio(), true);
    EXPECT_EQ(remote_sdp.HasVideo(), true);
    EXPECT_EQ(remote_sdp.media_count(), 3);

    EXPECT_NE(app, nullptr);
    EXPECT_EQ(app->mid(), "0");
    EXPECT_EQ(app->ice_ufrag().has_value(), true);
    EXPECT_EQ(app->ice_ufrag().value(), "KTqE");
    EXPECT_EQ(app->ice_pwd().has_value(), true);
    EXPECT_EQ(app->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_EQ(app->fingerprint().has_value(), true);
    EXPECT_EQ(app->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");

    EXPECT_NE(audio, nullptr);
    EXPECT_EQ(audio->mid(), "1");
    EXPECT_EQ(audio->ice_ufrag().has_value(), true);
    EXPECT_EQ(audio->ice_ufrag().value(), "KTqE");
    EXPECT_EQ(audio->ice_pwd().has_value(), true);
    EXPECT_EQ(audio->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_EQ(audio->fingerprint().has_value(), true);
    EXPECT_EQ(audio->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(audio->direction(), sdp::Direction::RECV_ONLY);

    EXPECT_NE(video, nullptr);
    EXPECT_EQ(video->mid(), "2");
    EXPECT_EQ(video->ice_ufrag().has_value(), true);
    EXPECT_EQ(video->ice_ufrag().value(), "KTqE");
    EXPECT_EQ(video->ice_pwd().has_value(), true);
    EXPECT_EQ(video->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_EQ(video->fingerprint().has_value(), true);
    EXPECT_EQ(video->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(video->direction(), sdp::Direction::RECV_ONLY);

}


} // namespace test
} // namespace naivertc