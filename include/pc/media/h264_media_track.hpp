#ifndef _PC_H264_MEDIA_TRACK_H_
#define _PC_H264_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "pc/media/media_track.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT H264MediaTrack : public MediaTrack {
public:
    H264MediaTrack(Config config);
    virtual ~H264MediaTrack();

    Kind kind() const override { return Kind::VIDEO; } 
    Codec codec() const override { return Codec::H264; } 

    int payload_type() const;
    void set_payload_type(int payload_type);
    std::optional<std::string> format_profile() const;

private:
    // TODO: Supports more payload types
    int payload_type_;

};
    
} // namespace naivertc


#endif