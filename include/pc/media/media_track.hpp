#ifndef _PC_MEDIA_TRACK_H_
#define _PC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "pc/sdp/sdp_entry.hpp"
#include "pc/sdp/sdp_defines.hpp"

#include <string>
#include <vector>

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
        uint32_t ssrc;
        std::string cname;
        std::string mid;
        std::string track_id;
        std::string msid; // media stream id
        Kind kind;
        Codec codec;
        std::vector<int> payload_types;
    };

public:
    MediaTrack(const sdp::Media& description);
    ~MediaTrack();

    std::string mid() const;
    sdp::Direction direction() const;
    sdp::Media description() const;
    
    void UpdateDescription(sdp::Media description);

public:
    static std::string kind_to_string(Kind kind);
    static std::string codec_to_string(Codec codec);
    static std::optional<std::string> FormatProfileForPayloadType(int payload_type);

protected:
    sdp::Media description_;
};

} // namespace naivertc


#endif