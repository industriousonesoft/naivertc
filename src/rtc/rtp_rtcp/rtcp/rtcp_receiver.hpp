#ifndef _RTC_RTP_RTCP_RTCP_RECEIVER_H_
#define _RTC_RTP_RTCP_RTCP_RECEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/ntp_time.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtcp/report_block_data.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/loss_notification.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

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
        TimeDelta min_rtt_ = TimeDelta::PlusInfinity();
        TimeDelta max_rtt_ = TimeDelta::MinusInfinity();
        TimeDelta sum_rtt_ = TimeDelta::Zero();
        size_t num_rtts_ = 0;
    };
public:
    RtcpReceiver(const RtcpConfiguration& config, 
                 Observer* const observer);
    ~RtcpReceiver();

    uint32_t local_media_ssrc() const;
    uint32_t remote_ssrc() const;

    void IncomingPacket(const uint8_t* packet, size_t packet_size) {
        IncomingPacket(CopyOnWriteBuffer(packet, packet + packet_size));
    }

    void IncomingPacket(CopyOnWriteBuffer packet);

    // Get received NTP.
    bool NTP(uint32_t* received_ntp_secs,
             uint32_t* received_ntp_frac,
             uint32_t* rtcp_arrival_time_secs,
             uint32_t* rtcp_arrival_time_frac,
             uint32_t* rtcp_timestamp,
             uint32_t* remote_sender_packet_count,
             uint64_t* remote_sender_octet_count,
             uint64_t* remote_sender_reports_count) const;

    int32_t RTT(uint32_t remote_ssrc,
                int64_t* last_rtt_ms,
                int64_t* avg_rtt_ms,
                int64_t* min_rtt_ms,
                int64_t* max_rtt_ms) const;

private:
    struct PacketInfo {
        // RTCP packet type bit field.
        uint32_t packet_type_flags = 0;
        uint32_t remote_ssrc = 0;
        int64_t rtt_ms = 0;
        // The receiver estimated max bitrate
        uint32_t remb_bps = 0;

        std::vector<uint16_t> nack_list;
        std::vector<ReportBlockData> report_block_datas;

        std::unique_ptr<rtcp::LossNotification> loss_notification;
    };

    bool ParseCompoundPacket(CopyOnWriteBuffer packet, 
                             PacketInfo* packet_info);
    bool ParseSenderReport(const rtcp::CommonHeader& rtcp_block, 
                           PacketInfo* packet_info);
    bool ParseReceiverReport(const rtcp::CommonHeader& rtcp_block, 
                             PacketInfo* packet_info);
    bool ParseSdes(const rtcp::CommonHeader& rtcp_block, 
                   PacketInfo* packet_info);
    bool ParseNack(const rtcp::CommonHeader& rtcp_block, 
                   PacketInfo* packet_info);
    bool ParseBye(const rtcp::CommonHeader& rtcp_block, 
                  PacketInfo* packet_info);
    void ParseReportBlock(const rtcp::ReportBlock& report_block, 
                          PacketInfo* packet_info,
                          uint32_t remote_ssrc);

    bool IsRegisteredSsrc(uint32_t ssrc) const;
private:
    Clock* const clock_;
    Observer* const observer_;
    bool receiver_only_;
    uint32_t remote_ssrc_;
    SequenceChecker sequence_checker_;

    std::map<int, uint32_t> registered_ssrcs_;
    std::map<uint32_t, ReportBlockData> received_report_blocks_;
    // Round-Trip Time per remote sender ssrc
    std::map<uint32_t, RttStats> rtts_;

    // Received sender report.
    NtpTime remote_sender_ntp_time_;
    uint32_t remote_sender_rtp_time_ = 0;
    // When did we receive the last send report.
    NtpTime last_received_sr_ntp_;
    uint32_t remote_sender_packet_count_ = 0;
    uint64_t remote_sender_octet_count_ = 0;
    uint64_t remote_sender_reports_count_ = 0;

    // The last time we received an RTCP Report block
    Timestamp last_received_rb_ = Timestamp::PlusInfinity();

    // The time we last received an RTCP RR telling we have successfully
    // delivered RTP packet to the remote side.
    Timestamp last_time_increased_sequence_number_ = Timestamp::PlusInfinity();

    RtcpNackStats nack_stats_;

    size_t num_skipped_packets_ = 0;
    int64_t last_skipped_packets_warning_ms_ = 0;
};
    
} // namespace naivertc


#endif