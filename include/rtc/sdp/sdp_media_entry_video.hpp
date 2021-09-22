#ifndef _RTC_SDP_MEDIA_ENTRY_VIDEO_H_
#define _RTC_SDP_MEDIA_ENTRY_VIDEO_H_

#include "rtc/sdp/sdp_media_entry_media.hpp"

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Video : public Media {
public: 
    Video(std::string mid, Direction direction = Direction::SEND_ONLY);
    ~Video();

    void AddCodec(int payload_type, const std::string codec, std::optional<const std::string> profile = std::nullopt);
};

} // namespace sdp
} // namespace naivert 

#endif