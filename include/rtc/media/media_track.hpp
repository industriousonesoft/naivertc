#ifndef _RTC_MEDIA_TRACK_H_
#define _RTC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/channels/media_channel.hpp"

#include <string>
#include <vector>
#include <optional>

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

    struct Configuration {
        std::string mid;
        
        Kind kind;
        Codec codec;
        std::vector<int> payload_types;

        uint32_t ssrc;
        std::optional<std::string> cname;
        std::optional<std::string> msid; // media stream id
        std::optional<std::string> track_id;

        Configuration(std::string mid, 
                Kind kind, 
                Codec codec, 
                std::vector<int> payload_types, 
                uint32_t ssrc, 
                std::optional<std::string> cname = std::nullopt, 
                std::optional<std::string> msid = std::nullopt,
                std::optional<std::string> track_id = std::nullopt);
    };
   
public:
    MediaTrack(sdp::Media description);
    ~MediaTrack();

    sdp::Direction direction() const;
    sdp::Media description() const;

    void UpdateDescription(sdp::Media description);
    
public:
    static std::string kind_to_string(Kind kind);
    static std::string codec_to_string(Codec codec);
    static std::optional<std::string> FormatProfileForPayloadType(int payload_type);
    static sdp::Media CreateDescription(const MediaTrack::Configuration& config);

private:
    sdp::Media description_;
};

} // namespace naivertc


#endif