#ifndef _RTC_RTCP_REPORT_BLOCK_H_
#define _RTC_RTCP_REPORT_BLOCK_H_

#include "base/defines.hpp"

namespace naivertc {
namespace rtcp {

class RTC_CPP_EXPORT ReportBlock {
public:
    static constexpr size_t kFixedReportBlockSize = 24;

    ReportBlock();
    ~ReportBlock();

    uint32_t ssrc() const { return ssrc_; }
    uint8_t fraction_lost() const { return fraction_lost_; }
    int32_t cumulative_packet_lost() const { return cumulative_packet_lost_; }
    uint16_t sequence_num_cycles() const { return seq_num_cycles_; }
    uint16_t highest_seq_num() const { return highest_seq_num_; }
    uint32_t jitter() const { return jitter_; }
    uint32_t last_sr_ntp_timestamp() const { return last_sr_ntp_timestamp_; }
    uint32_t delay_since_last_sr() const { return delay_since_last_sr_; }

    void set_ssrc(uint32_t ssrc) { ssrc_ = ssrc; }
    void set_fraction_lost(uint8_t fraction_lost) { fraction_lost_ = fraction_lost; }
    bool set_cumulative_packet_lost(int32_t cumulative_lost);
    void set_seq_num_cycles(uint16_t seq_num_cycles) { seq_num_cycles_ = seq_num_cycles; }
    void set_highest_sequence_num(uint16_t seq_num) { highest_seq_num_ = seq_num; }
    void set_extended_highest_sequence_num(uint32_t extended_seq_num);
    void set_jitter(uint32_t jitter) { jitter_ = jitter; }
    void set_last_sr_ntp_timestamp(uint32_t last_sr_ntp_timestamp) { last_sr_ntp_timestamp_ = last_sr_ntp_timestamp; }
    void set_delay_sr_since_last_sr(uint32_t delay_since_last_sr) { delay_since_last_sr_ = delay_since_last_sr; }

    bool Parse(const uint8_t* buffer, size_t size);
    bool PackInto(uint8_t* buffer, size_t size) const;

private:
     // ssrc
    uint32_t ssrc_;
    // fraction lost is high 8-bits value, cumulative packets lost is low signed 24-bits value
    uint8_t fraction_lost_;
    int32_t cumulative_packet_lost_;
    // the most significant 16 bits extend that sequence number 
    // with the corresponding count of sequence number cycles
    uint16_t seq_num_cycles_;
    // The low 16 bits contain the highest sequence number received in an
    // RTP data packet from source SSRC_n
    uint16_t highest_seq_num_;
    uint32_t jitter_;
    // Last send report timestamp, 
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