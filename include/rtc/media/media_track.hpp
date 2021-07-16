#ifndef _RTC_MEDIA_TRACK_H_
#define _RTC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_entry_media.hpp"
#include "rtc/sdp/sdp_defines.hpp"

#include <string>
#include <vector>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT MediaTrack : std::enable_shared_from_this<MediaTrack> {
public:
    enum class Kind {
        VIDEO,
        AUDIO
    };

    enum class Codec {
        H264,
        OPUS
    };

    struct RTC_CPP_EXPORT Config {
        std::string mid;
        
        Kind kind;
        Codec codec;
        std::vector<int> payload_types;

        uint32_t ssrc;
        std::optional<std::string> cname;
        std::optional<std::string> msid; // media stream id
        std::optional<std::string> track_id;

        Config(const std::string mid, 
                Kind kind, 
                Codec codec, 
                std::vector<int> payload_types, 
                uint32_t ssrc, 
                std::optional<std::string> cname = std::nullopt, 
                std::optional<std::string> msid = std::nullopt,
                std::optional<std::string> track_id = std::nullopt);
    };

public:
    MediaTrack(const sdp::Media& description);
    ~MediaTrack();

    std::string mid() const;
    sdp::Direction direction() const;
    sdp::Media description() const;

    void UpdateDescription(const sdp::Media& description);
    
public:
    static std::string kind_to_string(Kind kind);
    static std::string codec_to_string(Codec codec);
    static std::optional<std::string> FormatProfileForPayloadType(int payload_type);
    static sdp::Media BuildDescription(const MediaTrack::Config& config);

private:
    sdp::Media description_;
};

} // namespace naivertc


#endif