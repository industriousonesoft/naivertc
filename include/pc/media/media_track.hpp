#ifndef _PC_MEDIA_TRACK_H_
#define _PC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "pc/sdp/sdp_entry.hpp"

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
    MediaTrack(const Config& config);
    virtual ~MediaTrack() = default;

    virtual Kind kind() const = 0;

    virtual Codec codec() const = 0;

    virtual sdp::Media description() const = 0;

    uint32_t ssrc() const { return config_.ssrc; }
    std::string cname() const { return config_.cname; } 
    std::string mid() const { return config_.mid; }
    std::string track_id() const { return config_.track_id; }
    std::string msid() const { return config_.msid; }
    std::vector<int> payload_types() const { return config_.payload_types; }

public:
    static std::string kind_to_string(Kind kind);
    static std::string codec_to_string(Codec codec);

protected:
    virtual std::optional<std::string> FormatProfileForPayloadType(int payload_type) const = 0;

protected:
    const Config config_;
};

} // namespace naivertc


#endif