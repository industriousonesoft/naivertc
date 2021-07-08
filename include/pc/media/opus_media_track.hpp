#ifndef _PC_OPUS_MEDIA_TRACK_H_
#define _PC_OPUS_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "pc/media/media_track.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT OpusMediaTrack : public MediaTrack {
public:
    OpusMediaTrack(Config config, int sample_rate, int channels);
    virtual ~OpusMediaTrack();

    Kind kind() const override { return Kind::AUDIO; }
    Codec codec() const override { return Codec::OPUS; } 

    int sample_rate() const;
    int channels() const;

    int payload_type() const;
    void set_payload_type(int payload_type);
    std::optional<std::string> format_profile() const;

private:
    int payload_type_;
    int sample_rate_;
    int channels_;
};
    
} // namespace naivertc


#endif