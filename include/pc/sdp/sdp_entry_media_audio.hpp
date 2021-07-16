#ifndef _PC_SDP_ENTRY_MEDIA_AUDIO_H_
#define _PC_SDP_ENTRY_MEDIA_AUDIO_H_

#include "pc/sdp/sdp_entry_media.hpp"

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