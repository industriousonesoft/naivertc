#include "rtc/sdp/sdp_description.hpp"

#include <gtest/gtest.h>

#include <sstream>

namespace naivertc {
namespace test {

TEST(SDP_DescriptionTest, BuildAnOffer) {
    auto builder = sdp::Description::Builder(sdp::Type::OFFER)
                    .set_role(sdp::Role::ACT_PASS)
                    .set_ice_ufrag("KTqE")
                    .set_ice_pwd("u8XPW6fYzsDGjQmCYCQ+9W8S")
                    .set_fingerprint("8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    auto local_sdp = builder.Build();
    sdp::Application* app = local_sdp.SetApplication(sdp::Application("0"));
    sdp::Media* audio = local_sdp.AddMedia(sdp::Media(sdp::MediaEntry::Kind::AUDIO, "1", "UDP/TLS/RTP/SAVPF", sdp::Direction::SEND_RECV));
    audio->AddSsrc(18509423, sdp::Media::SsrcEntry::Kind::MEDIA, "sTjtznXLCNH7nbRw", "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C", "15598a91-caf9-4fff-a28f-3082310b2b7a");
    audio->AddSsrc(27389734, sdp::Media::SsrcEntry::Kind::FEC, "sTjtznXLCNH7nbRw", "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C", "15598a91-caf9-4fff-a28f-3082310b2b7a");

    sdp::Media* video = local_sdp.AddMedia(sdp::Media(sdp::MediaEntry::Kind::VIDEO, "2", "UDP/TLS/RTP/SAVPF", sdp::Direction::SEND_RECV));
    video->AddSsrc(3463951252, sdp::Media::SsrcEntry::Kind::MEDIA, "sTjtznXLCNH7nbRw", "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C", "ead4b4e9-b650-4ed5-86f8-6f5f5806346d");
    video->AddSsrc(1461041037, sdp::Media::SsrcEntry::Kind::RTX, "sTjtznXLCNH7nbRw", "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C", "ead4b4e9-b650-4ed5-86f8-6f5f5806346d");

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

    const sdp::Application* capp = local_sdp.application();
    const sdp::Media* caudio = local_sdp.media("1");
    const sdp::Media* cvideo = local_sdp.media("2");

    EXPECT_TRUE(capp != nullptr);
    EXPECT_EQ(capp->mid(), "0");
    EXPECT_TRUE(capp->ice_ufrag().has_value());
    EXPECT_EQ(capp->ice_ufrag().value(), "KTqE");
    EXPECT_TRUE(capp->ice_pwd().has_value());
    EXPECT_EQ(capp->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_TRUE(capp->fingerprint().has_value());
    EXPECT_EQ(capp->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");

    EXPECT_TRUE(caudio != nullptr);
    EXPECT_EQ(caudio->mid(), "1");
    EXPECT_TRUE(caudio->ice_ufrag().has_value());
    EXPECT_EQ(caudio->ice_ufrag().value(), "KTqE");
    EXPECT_TRUE(caudio->ice_pwd().has_value());
    EXPECT_EQ(caudio->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_TRUE(caudio->fingerprint().has_value());
    EXPECT_EQ(caudio->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(caudio->direction(), sdp::Direction::SEND_RECV);
    EXPECT_EQ(caudio->media_ssrcs().size(), 1);
    EXPECT_EQ(caudio->rtx_ssrcs().size(), 0);
    EXPECT_EQ(caudio->fec_ssrcs().size(), 1);
    EXPECT_TRUE(caudio->IsMediaSsrc(18509423));
    EXPECT_TRUE(caudio->IsFecSsrc(27389734));

    // Media ssrc
    const sdp::Media::SsrcEntry* audio_media_ssrc_entry = caudio->ssrc(18509423);
    EXPECT_TRUE(audio_media_ssrc_entry != nullptr);
    EXPECT_EQ(audio_media_ssrc_entry->ssrc, 18509423);
    EXPECT_EQ(audio_media_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::MEDIA);
    EXPECT_TRUE(audio_media_ssrc_entry->cname.has_value());
    EXPECT_EQ(audio_media_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(audio_media_ssrc_entry->msid.has_value());
    EXPECT_EQ(audio_media_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(audio_media_ssrc_entry->track_id.has_value());
    EXPECT_EQ(audio_media_ssrc_entry->track_id.value(), "15598a91-caf9-4fff-a28f-3082310b2b7a");
    // Fec ssrc
    const sdp::Media::SsrcEntry* audio_fec_ssrc_entry = caudio->ssrc(27389734);
    EXPECT_TRUE(audio_fec_ssrc_entry != nullptr);
    EXPECT_EQ(audio_fec_ssrc_entry->ssrc, 27389734);
    EXPECT_EQ(audio_fec_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::FEC);
    EXPECT_TRUE(audio_fec_ssrc_entry->cname.has_value());
    EXPECT_EQ(audio_fec_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(audio_fec_ssrc_entry->msid.has_value());
    EXPECT_EQ(audio_fec_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(audio_fec_ssrc_entry->track_id.has_value());
    EXPECT_EQ(audio_fec_ssrc_entry->track_id.value(), "15598a91-caf9-4fff-a28f-3082310b2b7a");

    EXPECT_TRUE(cvideo != nullptr);
    EXPECT_EQ(cvideo->mid(), "2");
    EXPECT_TRUE(cvideo->ice_ufrag().has_value());
    EXPECT_EQ(cvideo->ice_ufrag().value(), "KTqE");
    EXPECT_TRUE(cvideo->ice_pwd().has_value());
    EXPECT_EQ(cvideo->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_TRUE(cvideo->fingerprint().has_value());
    EXPECT_EQ(cvideo->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(cvideo->direction(), sdp::Direction::SEND_RECV);
    EXPECT_EQ(cvideo->media_ssrcs().size(), 1);
    EXPECT_EQ(cvideo->rtx_ssrcs().size(), 1);
    EXPECT_EQ(cvideo->fec_ssrcs().size(), 0);
    EXPECT_TRUE(cvideo->IsMediaSsrc(3463951252));
    EXPECT_TRUE(cvideo->IsRtxSsrc(1461041037));

    // Media ssrc
    const sdp::Media::SsrcEntry* video_media_ssrc_entry = cvideo->ssrc(3463951252);
    EXPECT_TRUE(video_media_ssrc_entry != nullptr);
    EXPECT_EQ(video_media_ssrc_entry->ssrc, 3463951252);
    EXPECT_EQ(video_media_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::MEDIA);
    EXPECT_TRUE(video_media_ssrc_entry->cname.has_value());
    EXPECT_EQ(video_media_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(video_media_ssrc_entry->msid.has_value());
    EXPECT_EQ(video_media_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(video_media_ssrc_entry->track_id.has_value());
    EXPECT_EQ(video_media_ssrc_entry->track_id.value(), "ead4b4e9-b650-4ed5-86f8-6f5f5806346d");
    // RTX ssrc
    const sdp::Media::SsrcEntry* video_rtx_ssrc_entry = cvideo->ssrc(1461041037);
    EXPECT_TRUE(video_rtx_ssrc_entry != nullptr);
    EXPECT_EQ(video_rtx_ssrc_entry->ssrc, 1461041037);
    EXPECT_EQ(video_rtx_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::RTX);
    EXPECT_TRUE(video_rtx_ssrc_entry->cname.has_value());
    EXPECT_EQ(video_rtx_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(video_rtx_ssrc_entry->msid.has_value());
    EXPECT_EQ(video_rtx_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(video_rtx_ssrc_entry->track_id.has_value());
    EXPECT_EQ(video_rtx_ssrc_entry->track_id.value(), "ead4b4e9-b650-4ed5-86f8-6f5f5806346d");
}

TEST(SDP_DescriptionTest, ParseAnOffer) {
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
    a=ssrc-group:FEC 18509423 27389734
    a=ssrc:18509423 cname:sTjtznXLCNH7nbRw
    a=ssrc:18509423 msid:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C 15598a91-caf9-4fff-a28f-3082310b2b7a
    a=ssrc:18509423 mslabel:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    a=ssrc:18509423 label:15598a91-caf9-4fff-a28f-3082310b2b7a
    a=ssrc:27389734 cname:sTjtznXLCNH7nbRw
    a=ssrc:27389734 msid:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C 15598a91-caf9-4fff-a28f-3082310b2b7a
    a=ssrc:27389734 mslabel:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    a=ssrc:27389734 label:15598a91-caf9-4fff-a28f-3082310b2b7a
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
    a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
    a=ssrc-group:FID 3463951252 1461041037
    a=ssrc:3463951252 cname:sTjtznXLCNH7nbRw
    a=ssrc:3463951252 msid:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C ead4b4e9-b650-4ed5-86f8-6f5f5806346d
    a=ssrc:3463951252 mslabel:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    a=ssrc:3463951252 label:ead4b4e9-b650-4ed5-86f8-6f5f5806346d
    a=ssrc:1461041037 cname:sTjtznXLCNH7nbRw
    a=ssrc:1461041037 msid:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C ead4b4e9-b650-4ed5-86f8-6f5f5806346d
    a=ssrc:1461041037 mslabel:h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    a=ssrc:1461041037 label:ead4b4e9-b650-4ed5-86f8-6f5f5806346d)";

    auto remote_sdp = sdp::Description::Parser::Parse(remote_sdp_string, sdp::Type::OFFER);
    
    EXPECT_EQ(remote_sdp.type(), sdp::Type::OFFER);
    EXPECT_EQ(remote_sdp.role(), sdp::Role::ACTIVE);
    EXPECT_TRUE(remote_sdp.ice_ufrag().has_value());
    EXPECT_TRUE(remote_sdp.ice_pwd().has_value());
    EXPECT_TRUE(remote_sdp.fingerprint().has_value());
    EXPECT_TRUE(remote_sdp.HasApplication());
    EXPECT_TRUE(remote_sdp.HasAudio());
    EXPECT_TRUE(remote_sdp.HasVideo());

    const sdp::Application* app = remote_sdp.application();
    const sdp::Media* audio = remote_sdp.media("1");
    const sdp::Media* video = remote_sdp.media("2");

    EXPECT_TRUE(app != nullptr);
    EXPECT_EQ(app->mid(), "0");
    EXPECT_TRUE(app->ice_ufrag().has_value());
    EXPECT_EQ(app->ice_ufrag().value(), "KTqE");
    EXPECT_TRUE(app->ice_pwd().has_value());
    EXPECT_EQ(app->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_TRUE(app->fingerprint().has_value());
    EXPECT_EQ(app->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");

    EXPECT_TRUE(audio != nullptr);
    EXPECT_EQ(audio->mid(), "1");
    EXPECT_TRUE(audio->ice_ufrag().has_value());
    EXPECT_EQ(audio->ice_ufrag().value(), "KTqE");
    EXPECT_TRUE(audio->ice_pwd().has_value());
    EXPECT_EQ(audio->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_TRUE(audio->fingerprint().has_value());
    EXPECT_EQ(audio->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(audio->direction(), sdp::Direction::RECV_ONLY);
    EXPECT_EQ(audio->media_ssrcs().size(), 1);
    EXPECT_EQ(audio->rtx_ssrcs().size(), 0);
    EXPECT_EQ(audio->fec_ssrcs().size(), 1);
    EXPECT_TRUE(audio->IsMediaSsrc(18509423));
    EXPECT_TRUE(audio->IsFecSsrc(27389734));
    // Media ssrc
    const sdp::Media::SsrcEntry* audio_media_ssrc_entry = audio->ssrc(18509423);
    EXPECT_TRUE(audio_media_ssrc_entry != nullptr);
    EXPECT_EQ(audio_media_ssrc_entry->ssrc, 18509423);
    EXPECT_EQ(audio_media_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::MEDIA);
    EXPECT_TRUE(audio_media_ssrc_entry->cname.has_value());
    EXPECT_EQ(audio_media_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(audio_media_ssrc_entry->msid.has_value());
    EXPECT_EQ(audio_media_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(audio_media_ssrc_entry->track_id.has_value());
    EXPECT_EQ(audio_media_ssrc_entry->track_id.value(), "15598a91-caf9-4fff-a28f-3082310b2b7a");
    // Fec ssrc
    const sdp::Media::SsrcEntry* audio_fec_ssrc_entry = audio->ssrc(27389734);
    EXPECT_TRUE(audio_fec_ssrc_entry != nullptr);
    EXPECT_EQ(audio_fec_ssrc_entry->ssrc, 27389734);
    EXPECT_EQ(audio_fec_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::FEC);
    EXPECT_TRUE(audio_fec_ssrc_entry->cname.has_value());
    EXPECT_EQ(audio_fec_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(audio_fec_ssrc_entry->msid.has_value());
    EXPECT_EQ(audio_fec_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(audio_fec_ssrc_entry->track_id.has_value());
    EXPECT_EQ(audio_fec_ssrc_entry->track_id.value(), "15598a91-caf9-4fff-a28f-3082310b2b7a");

    EXPECT_TRUE(video != nullptr);
    EXPECT_EQ(video->mid(), "2");
    EXPECT_TRUE(video->ice_ufrag().has_value());
    EXPECT_EQ(video->ice_ufrag().value(), "KTqE");
    EXPECT_TRUE(video->ice_pwd().has_value());
    EXPECT_EQ(video->ice_pwd().value(), "u8XPW6fYzsDGjQmCYCQ+9W8S");
    EXPECT_TRUE(video->fingerprint().has_value());
    EXPECT_EQ(video->fingerprint().value(), "8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    EXPECT_EQ(video->direction(), sdp::Direction::RECV_ONLY);
    EXPECT_EQ(video->media_ssrcs().size(), 1);
    EXPECT_EQ(video->rtx_ssrcs().size(), 1);
    EXPECT_EQ(video->fec_ssrcs().size(), 0);
    EXPECT_TRUE(video->IsMediaSsrc(3463951252));
    EXPECT_TRUE(video->IsRtxSsrc(1461041037));
    // Media ssrc
    const sdp::Media::SsrcEntry* video_media_ssrc_entry = video->ssrc(3463951252);
    EXPECT_TRUE(video_media_ssrc_entry != nullptr);
    EXPECT_EQ(video_media_ssrc_entry->ssrc, 3463951252);
    EXPECT_EQ(video_media_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::MEDIA);
    EXPECT_TRUE(video_media_ssrc_entry->cname.has_value());
    EXPECT_EQ(video_media_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(video_media_ssrc_entry->msid.has_value());
    EXPECT_EQ(video_media_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(video_media_ssrc_entry->track_id.has_value());
    EXPECT_EQ(video_media_ssrc_entry->track_id.value(), "ead4b4e9-b650-4ed5-86f8-6f5f5806346d");
    // RTX ssrc
    const sdp::Media::SsrcEntry* video_rtx_ssrc_entry = video->ssrc(1461041037);
    EXPECT_TRUE(video_rtx_ssrc_entry != nullptr);
    EXPECT_EQ(video_rtx_ssrc_entry->ssrc, 1461041037);
    EXPECT_EQ(video_rtx_ssrc_entry->kind, sdp::Media::SsrcEntry::Kind::RTX);
    EXPECT_TRUE(video_rtx_ssrc_entry->cname.has_value());
    EXPECT_EQ(video_rtx_ssrc_entry->cname.value(), "sTjtznXLCNH7nbRw");
    EXPECT_TRUE(video_rtx_ssrc_entry->msid.has_value());
    EXPECT_EQ(video_rtx_ssrc_entry->msid.value(), "h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C");
    EXPECT_TRUE(video_rtx_ssrc_entry->track_id.has_value());
    EXPECT_EQ(video_rtx_ssrc_entry->track_id.value(), "ead4b4e9-b650-4ed5-86f8-6f5f5806346d");

}

} // namespace test
} // namespace naivertc