#ifndef _RTC_MEDIA_TRACK_H_
#define _RTC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/channels/media_channel.hpp"
#include "rtc/call/rtp_packet_sink.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <iostream>

namespace naivertc {

class RtpPacketReceived;
class CopyOnWriteBuffer;

class RTC_CPP_EXPORT MediaTrack : public MediaChannel,
                                  public RtpPacketSink,
                                  public std::enable_shared_from_this<MediaTrack> {
public:
    using Direction = sdp::Direction;
    enum class Codec {
        H264,
        OPUS
    };

    enum class FecCodec {
        // UlpFec + Red
        ULP_FEC,
        // FlexFec + Ssrc
        FLEX_FEC
    };

    enum class RtcpFeedback {
        NACK
        // TODO: Support more feedback
        // goog-remb
        // transport-cc
    };

    struct CodecParams {
        Codec codec;
        // TODO: Add more attributes, e.g.: clock rate, channels.
        std::optional<std::string> profile = std::nullopt;

        CodecParams(Codec codec,
                    std::optional<std::string> profile = std::nullopt);
    };

    struct Configuration {
    public:
        Configuration(Kind kind, std::string mid);

        Kind kind() const { return kind_; };
        std::string mid() const { return mid_; }
        void set_mid(std::string mid) { mid_ = std::move(mid); }

        // Media codec
        bool AddCodec(CodecParams cp);
        bool AddCodec(Codec codec, std::optional<std::string> profile = std::nullopt);
        void RemoveCodec(Codec codec, std::optional<std::string> profile = std::nullopt);
        void ForEachCodec(std::function<void(const CodecParams& cp)>&& handler) const;
        std::vector<CodecParams> media_codecs() const { return media_codecs_; }
        
        // Feedbacks
        void AddFeedback(RtcpFeedback fb);
        void RemoveFeedback(RtcpFeedback fb);
        void ForEachFeedback(std::function<void(RtcpFeedback cp)>&& handler) const;
        std::vector<RtcpFeedback> rtcp_feedbacks() const { return rtcp_feedbacks_; }

    public:
        Direction direction = Direction::SEND_RECV;
        bool rtx_enabled = false;
        std::optional<FecCodec> fec_codec = std::nullopt;

        std::optional<std::string> cname = std::nullopt;
        std::optional<std::string> msid = std::nullopt;
        std::optional<std::string> track_id = std::nullopt;
    
    private:
        Kind kind_;
        std::string mid_;
    
        std::vector<CodecParams> media_codecs_;
        std::vector<RtcpFeedback> rtcp_feedbacks_;
    };
    
public:
    MediaTrack(const Configuration& config);
    MediaTrack(sdp::Media description);
    ~MediaTrack();

    const sdp::Media* local_description() const;
    bool ReconfigLocalDescription(const Configuration& config);

    const sdp::Media* remote_description() const;
    bool OnRemoteDescription(sdp::Media description);

    void OnRtpPacket(RtpPacketReceived in_packet) override;
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;
    
private:
    class SDPBuilder {
    public:
        static std::optional<sdp::Media> Build(const Configuration& config);
    private:
        static bool AddCodecs(const Configuration& config, sdp::Media& media);
        static bool AddMediaCodec(int payload_type, const CodecParams& cp, sdp::Media& media);
        static bool AddFeedback(int payload_type, RtcpFeedback fb, sdp::Media& media);
        static bool AddSsrcs(const Configuration& config, sdp::Media& media);
        static std::optional<int> NextPayloadType(Kind kind);
    };
private:
    std::optional<sdp::Media> local_description_;
    std::optional<sdp::Media> remote_description_;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::Codec codec);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::FecCodec codec);

} // namespace naivertc


#endif