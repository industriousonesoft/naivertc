#include "sdp_description_examples.hpp"

#include <rtc/sdp/sdp_description.hpp>
#include <rtc/sdp/sdp_media_entry_media.hpp>

#include <iostream>

namespace sdptest {

using namespace naivertc;

void BuildAnOffer() {
    auto builder = sdp::Description::Builder(sdp::Type::OFFER)
                    .set_role(sdp::Role::ACT_PASS)
                    .set_ice_ufrag("KTqE")
                    .set_ice_pwd("u8XPW6fYzsDGjQmCYCQ+9W8S")
                    .set_fingerprint("8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB");
    auto local_sdp = builder.Build();
    local_sdp.SetApplication("0");
    auto audio = local_sdp.AddAudio("1", "UDP/TLS/RTP/SAVPF", sdp::Direction::SEND_RECV);
    audio->AddAudioCodec(111, "OPUS", 48000, 2, "minptime=10;useinbandfec=1");
    auto video = local_sdp.AddVideo("2", "UDP/TLS/RTP/SAVPF", sdp::Direction::SEND_RECV);
    video->AddVideoCodec(102, "H264", "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1");

    auto sdp_string = local_sdp.GenerateSDP("\n");

    std::cout << "Local sdp: \n" << sdp_string << std::endl;
}

void ParseAnAnswer() {
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
    a=mid:2
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
    a=mid:1
    a=recvonly
    a=rtcp-mux
    a=rtpmap:102 h264/90000
    a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f)";

    auto remote_sdp = sdp::Description::Parser::Parse(remote_sdp_string, sdp::Type::OFFER);

    auto sdp_string = remote_sdp.GenerateSDP("\n");

    std::cout << "Remote sdp: \n" << sdp_string << std::endl;
}

}