#ifndef _PC_OPUS_MEDIA_TRACK_H_
#define _PC_OPUS_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "pc/media/media_track.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT OpusMediaTrack : public MediaTrack {
public:
    OpusMediaTrack(const Config& config, int sample_rate, int channels);
    virtual ~OpusMediaTrack();

    Kind kind() const override { return Kind::AUDIO; }
    Codec codec() const override { return Codec::OPUS; } 

    sdp::Media description() const override;

    int sample_rate() const;
    int channels() const;

private:
    std::optional<std::string> FormatProfileForPayloadType(int payload_type) const override;
   
private:
    int sample_rate_;
    int channels_;

    sdp::Audio description_;
};
    
} // namespace naivertc


#endif