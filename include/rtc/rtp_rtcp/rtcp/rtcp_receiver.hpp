#ifndef _RTC_RTP_RTCP_RTCP_RECEIVER_H_
#define _RTC_RTP_RTCP_RTCP_RECEIVER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/ntp_time.hpp"
#include "rtc/base/timestamp.hpp"
#include "rtc/base/time_delta.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtcp/report_block_data.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"

#include <vector>
#include <map>
#include <string>

namespace naivertc {

namespace rtcp {
class CommonHeader;
class TmmbItem;
class ReportBlock;
};

// RtcpReceiver
class RTC_CPP_EXPORT RtcpReceiver {
public:
    // Observer
    class Observer {
    public:
        virtual ~Observer() = default;
        
        virtual void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) = 0;
        virtual void OnRequestSendReport() = 0;
        virtual void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) = 0;
        virtual void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) = 0;  
    };
public:
    // RttStats
    class RttStats {
    public:
        RttStats() = default;
        RttStats(const RttStats&) = default;
        RttStats& operator=(const RttStats&) = default;

        void AddRtt(TimeDelta rtt);

        TimeDelta last_rtt() const { return last_rtt_; }
        TimeDelta min_rtt() const { return min_rtt_; }
        TimeDelta max_rtt() const { return max_rtt_; }
        TimeDelta average_rtt() const { return sum_rtt_ / num_rtts_; }

    private:
        TimeDelta last_rtt_ = TimeDelta::Zero();
        TimeDelta min_rtt_ = TimeDelta::MaxValue();
        TimeDelta max_rtt_ = TimeDelta::MinValue();
        TimeDelta sum_rtt_ = TimeDelta::Zero();
        size_t num_rtts_ = 0;
    };
public:
    RtcpReceiver(const RtcpConfiguration& config, 
                 Observer* const observer,
                 std::shared_ptr<TaskQueue> task_queue);
    ~RtcpReceiver();

    void set_local_media_ssrc(uint32_t ssrc);
    uint32_t local_media_ssrc() const;

    void set_remote_ssrc(uint32_t ssrc);
    uint32_t remote_ssrc() const;

    void IncomingPacket(const uint8_t* packet, size_t packet_size) {
        IncomingPacket(BinaryBuffer(packet, packet + packet_size));
    }

    void IncomingPacket(BinaryBuffer packet);

    // Get received NTP.
    bool NTP(uint32_t* received_ntp_secs,
             uint32_t* received_ntp_frac,
             uint32_t* rtcp_arrival_time_secs,
             uint32_t* rtcp_arrival_time_frac,
             uint32_t* rtcp_timestamp,
             uint32_t* remote_sender_packet_count,
             uint64_t* remote_sender_octet_count,
             uint64_t* remote_sender_reports_count) const;

private:
    bool ParseCompoundPacket(BinaryBuffer packet);
    bool ParseSenderReport(const rtcp::CommonHeader& rtcp_block);
    bool ParseReceiverReport(const rtcp::CommonHeader& rtcp_block);
    bool ParseSdes(const rtcp::CommonHeader& rtcp_block);
    bool ParseNack(const rtcp::CommonHeader& rtcp_block);
    bool ParseBye(const rtcp::CommonHeader& rtcp_block);
    
    void HandleReportBlock(const rtcp::ReportBlock& report_block, uint32_t remote_ssrc);

    bool IsRegisteredSsrc(uint32_t ssrc) const;

private:
    std::shared_ptr<Clock> clock_;
    Observer* const observer_;
    bool receiver_only_;
    std::shared_ptr<TaskQueue> task_queue_;

    std::map<int, uint32_t> registered_ssrcs_;
    std::map<uint32_t, ReportBlockData> received_report_blocks_;
    // Round-Trip Time per remote sender ssrc
    std::map<uint32_t, RttStats> rtts_;

    uint32_t remote_ssrc_ = 0;

    // Received sender report.
    NtpTime remote_sender_ntp_time_;
    uint32_t remote_sender_rtp_time_ = 0;
    // When did we receive the last send report.
    NtpTime last_received_sr_ntp_;
    uint32_t remote_sender_packet_count_ = 0;
    uint64_t remote_sender_octet_count_ = 0;
    uint64_t remote_sender_reports_count_ = 0;

    // The last time we received an RTCP Report block
    Timestamp last_time_received_rb_ = Timestamp::MaxValue();

    // The time we last received an RTCP RR telling we have successfully
    // delivered RTP packet to the remote side.
    Timestamp last_time_increased_sequence_number_ = Timestamp::MaxValue();

    RtcpNackStats nack_stats_;

    size_t num_skipped_packets_ = 0;
    int64_t last_skipped_packets_warning_ms_ = 0;
};
    
} // namespace naivertc


#endif