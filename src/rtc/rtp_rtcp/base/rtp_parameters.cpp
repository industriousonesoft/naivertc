#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"

namespace naivertc {

std::unordered_map<int, int> RtpParameters::rtx_associated_payload_types() const {
    std::unordered_map<int, int> associated_types;
    // Media type
    if (media_rtx_payload_type) {
        associated_types[*media_rtx_payload_type] = media_payload_type;
    }
    // RED
    if (ulpfec.red_rtx_payload_type) {
        associated_types[*ulpfec.red_rtx_payload_type] = ulpfec.red_payload_type;
    }
    return associated_types;
}
    
} // namespace naivertc
