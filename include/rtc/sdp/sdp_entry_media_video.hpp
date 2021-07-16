#ifndef _RTC_SDP_ENTRY_MEDIA_VIDEO_H_
#define _RTC_SDP_ENTRY_MEDIA_VIDEO_H_

#include "rtc/sdp/sdp_entry_media.hpp"

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Video : public Media {
public: 
    Video(std::string mid, Direction direction = Direction::SEND_ONLY);

    void AddCodec(int payload_type, std::string codec, std::optional<std::string> profile = std::nullopt);
};

} // namespace sdp
} // namespace naivert 

#endif