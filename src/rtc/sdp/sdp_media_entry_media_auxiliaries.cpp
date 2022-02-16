#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_string.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace sdp {

// ExtMap
Media::ExtMap::ExtMap(int id, std::string uri) 
    : id(id),
      uri(uri) {}

// RtpMap
Media::RtpMap::RtpMap(int payload_type, 
                     Codec codec, 
                     int clock_rate, 
                     std::optional<std::string> codec_params,
                     std::optional<int> rtx_payload_type) 
    : payload_type(payload_type),
      codec(codec),
      clock_rate(clock_rate),
      codec_params(codec_params),
      rtx_payload_type(rtx_payload_type) {}

// SsrcEntry
Media::SsrcEntry::SsrcEntry(uint32_t ssrc,
                            Kind kind,
                            std::optional<std::string> cname, 
                            std::optional<std::string> msid, 
                            std::optional<std::string> track_id) 
    : ssrc(ssrc),
      kind(kind),
      cname(cname),
      msid(msid),
      track_id(track_id) {}
    
} // namespace sdp
} // namespace naivertc