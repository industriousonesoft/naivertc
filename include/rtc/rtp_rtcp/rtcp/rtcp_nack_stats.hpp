#ifndef _RTC_RTP_RTCP_RTCP_NACK_STATS_H_
#define _RTC_RTP_RTCP_RTCP_NACK_STATS_H_

#include "base/defines.hpp"

#include <stdint.h>

namespace naivertc {

class RTC_CPP_EXPORT RtcpNackStats {
public:
    RtcpNackStats();

    // Updates stats with requested sequence number.
    // This function should be called for each NACK request to calculate the
    // number of unique NACKed RTP packets.
    void ReportRequest(uint16_t sequence_number);

    // Gets the number of NACKed RTP packets.
    uint32_t requests() const { return requests_; }

    // Gets the number of unique NACKed RTP packets.
    uint32_t unique_requests() const { return unique_requests_; }

private:
    uint16_t max_sequence_number_;
    uint32_t requests_;
    uint32_t unique_requests_;
};
    
} // namespace naivertc


#endif