#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_REPORT_BLOCK_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_REPORT_BLOCK_H_

#include "base/defines.hpp"

namespace naivertc {
namespace rtcp {

// A ReportBlock represents the Sender Report packet from
// RFC 3550 section 6.4.1.
class RTC_CPP_EXPORT ReportBlock {
public:
    static constexpr size_t kFixedReportBlockSize = 24;

    ReportBlock();
    ~ReportBlock();

    uint32_t source_ssrc() const { return source_ssrc_; }
    uint8_t fraction_lost() const { return fraction_lost_; }
    int32_t cumulative_packet_lost() const { return cumulative_packet_lost_; }
    uint16_t sequence_num_cycles() const;
    uint16_t highest_seq_num() const;
    uint32_t extended_highest_seq_num() const { return extended_highest_seq_num_; }
    uint32_t jitter() const { return jitter_; }
    uint32_t last_sr_ntp_timestamp() const { return last_sr_ntp_timestamp_; }
    uint32_t delay_since_last_sr() const { return delay_since_last_sr_; }

    void set_media_ssrc(uint32_t ssrc) { source_ssrc_ = ssrc; }
    void set_fraction_lost(uint8_t fraction_lost) { fraction_lost_ = fraction_lost; }
    bool set_cumulative_packet_lost(int32_t cumulative_lost);
    void set_extended_highest_sequence_num(uint32_t extended_seq_num);
    void set_jitter(uint32_t jitter) { jitter_ = jitter; }
    void set_last_sr_ntp_timestamp(uint32_t last_sr_ntp_timestamp) { last_sr_ntp_timestamp_ = last_sr_ntp_timestamp; }
    void set_delay_sr_since_last_sr(uint32_t delay_since_last_sr) { delay_since_last_sr_ = delay_since_last_sr; }

    bool Parse(const uint8_t* buffer, size_t size);
    bool PackInto(uint8_t* buffer, size_t size) const;

private:
     // ssrc of source
    uint32_t source_ssrc_;
    // fraction lost is high 8-bits value, cumulative packets lost is low signed 24-bits value
    uint8_t fraction_lost_;
    int32_t cumulative_packet_lost_;
    uint32_t extended_highest_seq_num_;
    uint32_t jitter_;
    // Last send report NTP timestamp, 
    // the middle 32 bits out of 64 in the NTP timestamp
    uint32_t last_sr_ntp_timestamp_;
    // The delay, expressed in units of 1/65536 seconds, between
    // receiving the last SR packet from source SSRC_n and sending this
    // reception report block. 
    uint32_t delay_since_last_sr_;
};
    
} // namespace rtcp    
} // namespace naivert 


#endif