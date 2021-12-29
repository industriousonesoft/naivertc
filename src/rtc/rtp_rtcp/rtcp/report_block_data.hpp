#ifndef _RTC_RTP_RTCP_REPORT_BLOCK_DATA_H_
#define _RTC_RTP_RTCP_REPORT_BLOCK_DATA_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_structs.hpp"

namespace naivertc {

class RTC_CPP_EXPORT ReportBlockData {
public:
    ReportBlockData();

    const RTCPReportBlock& report_block() const { return report_block_; }
    int64_t report_block_timestamp_utc_us() const {
        return report_block_timestamp_utc_us_;
    }
    int64_t last_rtt_ms() const { return last_rtt_ms_; }
    int64_t min_rtt_ms() const { return min_rtt_ms_; }
    int64_t max_rtt_ms() const { return max_rtt_ms_; }
    int64_t sum_rtt_ms() const { return sum_rtt_ms_; }
    size_t num_rtts() const { return num_rtts_; }
    bool has_rtt() const { return num_rtts_ != 0; }

    double AvgRttMs() const;

    void SetReportBlock(RTCPReportBlock report_block, 
                        int64_t report_block_timestamp_utc_us);
                        
    void AddRttMs(int64_t rtt_ms);

private:
    RTCPReportBlock report_block_;
    int64_t report_block_timestamp_utc_us_;

    int64_t last_rtt_ms_;
    int64_t min_rtt_ms_;
    int64_t max_rtt_ms_;
    int64_t sum_rtt_ms_;
    size_t num_rtts_;
};
    
} // namespace naivertc


#endif