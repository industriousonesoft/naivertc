#ifndef _PC_H264_MEDIA_TRACK_H_
#define _PC_H264_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "pc/media/media_track.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT H264MediaTrack : public MediaTrack {
public:
    H264MediaTrack(const Config& config);
    virtual ~H264MediaTrack();

    Kind kind() const override { return Kind::VIDEO; } 
    Codec codec() const override { return Codec::H264; }

    sdp::Media description() const override;

private:
    std::optional<std::string> FormatProfileForPayloadType(int payload_type) const override;

private:
    sdp::Video description_;
};
    
} // namespace naivertc


#endif