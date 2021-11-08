#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

namespace naivertc {

RtcpNackStats::RtcpNackStats()
    : max_sequence_number_(0), requests_(0), unique_requests_(0) {}

void RtcpNackStats::ReportRequest(uint16_t sequence_number) {
    if (requests_ == 0 || wrap_around_utils::AheadOf<uint16_t>(sequence_number, max_sequence_number_)) {
        max_sequence_number_ = sequence_number;
        ++unique_requests_;
    }
    ++requests_;
}
    
} // namespace naivertc
