#include "rtc/rtp_rtcp/rtp/fec/fec_receiver_ulp.hpp"

namespace naivertc {

UlpFecReceiver::UlpFecReceiver(uint32_t ssrc) 
 : ssrc_(ssrc) {}

UlpFecReceiver::~UlpFecReceiver() {}
    
} // namespace naivertc
