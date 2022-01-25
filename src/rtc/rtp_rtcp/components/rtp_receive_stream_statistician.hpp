#ifndef _RTC_RTP_RTCP_COMPONENTS_STREAM_STATISTICIAN_H_
#define _RTC_RTP_RTCP_COMPONENTS_STREAM_STATISTICIAN_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_structs.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"
#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpReceiveStreamStatistician {
public:
    RtpReceiveStreamStatistician(uint32_t ssrc, Clock* clock, int max_reordering_threshold);
    ~RtpReceiveStreamStatistician();

    void set_max_reordering_threshold(int threshold);
    void set_enable_retransmit_detection(bool enable);

    void OnRtpPacket(const RtpPacketReceived& packet);

    std::optional<rtcp::ReportBlock> GetReportBlock();
    RtpReceiveStats GetStates() const;
    std::optional<int> GetFractionLostInPercent() const;
    RtpStreamDataCounters GetReceiveStreamDataCounters() const;
    std::optional<DataRate> GetReceivedBitrate();

private:
    bool HasReceivedRtpPacket() const;

    bool IsRetransmitedPacket(const RtpPacketReceived& packet, 
                                 int64_t receive_time_ms) const;

    void UpdateJitter(const RtpPacketReceived& packet, 
                      int64_t receive_time_ms);

    // Return true on the incoming packet is considered to be out of order.
    bool IsOutOfOrderPacket(const RtpPacketReceived& packet, 
                            int64_t seq_num, 
                            int64_t receive_time_ms);

private:
    const uint32_t ssrc_;
    Clock* const clock_;
    // Delta used to map internal timestamps to Unix epoch ones.
    const int64_t delta_internal_unix_epoch_ms_;

    int max_reordering_threshold_;
    bool enable_retransmit_detection_;
    bool cumulative_loss_is_capped_;

    // Stats on received RTP packets.
    uint32_t jitter_q4_;
    // Cumulative loss according to RFC 3550, which may be negative (and often is,
    // if packets are reordered and there are non-RTX retransmissions).
    int32_t cumulative_loss_;
    // Offset added to outgoing rtcp reports, to make ensure that the reported
    // cumulative loss is non-negative. Reports with negative values confuse some
    // senders, in particular, our own loss-based bandwidth estimator.
    int32_t cumulative_loss_rtcp_offset_;

    int64_t last_receive_time_ms_;
    uint32_t last_packet_timestamp_;
    int64_t first_received_seq_num_;
    int64_t last_received_seq_num_;
    SeqNumUnwrapper seq_unwrapper_;

    // Assume that the other side restarted when there are two sequential packets
    // with large jump from received_seq_max_.
    std::optional<uint16_t> received_seq_out_of_order_;

    // Counter values when we sent the last report.
    int32_t last_report_cumulative_loss_;
    int64_t last_report_max_seq_num_;

    RtpStreamDataCounters receive_counters_;
    BitRateStatistics bitrate_stats_;
};
    
} // namespace naivertc


#endif