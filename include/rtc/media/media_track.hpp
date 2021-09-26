#ifndef _RTC_MEDIA_TRACK_H_
#define _RTC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/channels/media_channel.hpp"

#include <string>
#include <vector>
#include <optional>

#include <iostream>

namespace naivertc {

class RTC_CPP_EXPORT MediaTrack : public MediaChannel,
                                  public std::enable_shared_from_this<MediaTrack> {
public:
    enum class Kind {
        VIDEO,
        AUDIO
    };

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

    struct Configuration {
        std::string mid;
        Kind kind;
        Codec codec;
     
        bool nack_enabled = false;
        bool rtx_enabled = false;
        std::optional<FecCodec> fec_codec = std::nullopt;

        std::optional<std::string> cname = std::nullopt;
        std::optional<std::string> msid = std::nullopt; 
        std::optional<std::string> track_id = std::nullopt;

        Configuration(std::string mid, Kind kind, Codec codec);
    };

public:
    static std::optional<sdp::Media> BuildDescription(const MediaTrack::Configuration& config);
   
public:
    MediaTrack(sdp::Media description);
    ~MediaTrack();

    sdp::Direction direction() const;
    
    sdp::Media description() const;
    void set_description(sdp::Media description);

private:
    static std::optional<int> NextPayloadType(Kind kind);
    
private:
    sdp::Media description_;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::Kind kind);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::Codec codec);

} // namespace naivertc


#endif