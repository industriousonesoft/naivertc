#ifndef _RTC_SDP_MEDIA_ENTRY_AUDIO_H_
#define _RTC_SDP_MEDIA_ENTRY_AUDIO_H_

#include "rtc/sdp/sdp_media_entry_media.hpp"

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Audio : public Media {
public:
    Audio(std::string mid, Direction direction = Direction::SEND_ONLY);

    void AddCodec(int payload_type, std::string codec, int clock_rate = 48000, int channels = 2, std::optional<std::string> profile = std::nullopt);

};

} // namespace sdp
} // namespace naivert 

#endif