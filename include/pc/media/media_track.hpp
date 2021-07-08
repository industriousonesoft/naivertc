#ifndef _PC_MEDIA_TRACK_H_
#define _PC_MEDIA_TRACK_H_

#include "base/defines.hpp"

#include <string>

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

    struct Config {
        uint32_t ssrc;
        std::string cname;
        std::string mid;
        std::string track_id;
        std::string msid; // media stream id
    };

public:
    MediaTrack(Config config);
    virtual ~MediaTrack() = default;

    virtual Kind kind() const = 0;
    std::string kind_string() const;

    virtual Codec codec() const = 0;
    std::string codec_string() const;

    uint32_t ssrc() const { return config_.ssrc; }
    std::string cname() const { return config_.cname; } 
    std::string mid() const { return config_.mid; }
    std::string track_id() const { return config_.track_id; }
    std::string msid() const { return config_.msid; }

protected:
    Config config_;
};

} // namespace naivertc


#endif