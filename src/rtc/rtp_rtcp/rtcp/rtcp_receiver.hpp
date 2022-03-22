#ifndef _RTC_RTP_RTCP_RTCP_RECEIVER_H_
#define _RTC_RTP_RTCP_RTCP_RECEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/ntp_time.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/loss_notification.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/rtpfb.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/nack.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/psfb.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/tmmbr.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/tmmbn.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/bye.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/remb.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/extended_reports.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"

#include <vector>
#include <unordered_map>
#include <string>
#include <list>

namespace naivertc {

namespace rtcp {
class CommonHeader;
class TmmbItem;
class ReportBlock;
};

// RtcpReceiver
class RtcpReceiver {
public:
    RtcpReceiver(const RtcpConfiguration& config);
    ~RtcpReceiver();

    uint32_t local_media_ssrc() const;
    uint32_t remote_ssrc() const;
    void set_remote_ssrc(uint32_t remote_ssrc);

    TimeDelta rtt() const;

    void IncomingRtcpPacket(CopyOnWriteBuffer packet);

    std::optional<RtcpSenderReportStats> GetLastSenderReportStats() const;

    std::optional<RttStats> GetRttStats(uint32_t ssrc) const;

    std::vector<RtcpReportBlock> GetLatestReportBlocks() const;

    std::optional<TimeDelta> GetLatestXrRrRtt() const;

    std::vector<rtcp::Dlrr::TimeInfo> ConsumeXrDlrrTimeInfos();

    int64_t LastReceivedReportBlockMs() const;

    // Returns true if we haven't received an RTCP RR for several RTCP
    // intervals, but only triggers true once.
    bool RtcpRrTimeout();

    // Returns true if we haven't received an RTCP RR telling the receive side
    // has not received RTP packets for too long, i.e. extended highest sequence
    // number hasn't increased for several RTCP intervals. The function only
    // returns true once until a new RR is received.
    bool RtcpRrSequenceNumberTimeout();

private:
    struct PacketInfo {
        // RTCP packet type bit field.
        uint32_t packet_type_flags = 0;
        uint32_t remote_ssrc = 0;
        int64_t rtt_ms = 0;
        // The receiver estimated max bitrate
        uint32_t remb_bps = 0;

        std::vector<uint16_t> nack_list;
        std::vector<RtcpReportBlock> report_blocks;

        RttStats rtt_stats;
    };

    struct RrtrInfo {
        RrtrInfo() 
            : ssrc(0),
              received_remote_mid_ntp_time(0),
              local_receive_mid_ntp_time(0) {}

        RrtrInfo(uint32_t ssrc,
                 uint32_t received_remote_mid_ntp_time,
                 uint32_t local_receive_mid_ntp_time) 
            : ssrc(ssrc),
              received_remote_mid_ntp_time(received_remote_mid_ntp_time),
              local_receive_mid_ntp_time(local_receive_mid_ntp_time) {};

        uint32_t ssrc;
        uint32_t received_remote_mid_ntp_time;
        // NTP time when the report was received.
        uint32_t local_receive_mid_ntp_time;
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
    bool ParseTransportFeedback(const rtcp::CommonHeader& rtcp_block, 
                                PacketInfo* packet_info);
    bool ParsePli(const rtcp::CommonHeader& rtcp_block,
                  PacketInfo* packet_info);
    bool ParseFir(const rtcp::CommonHeader& rtcp_block,
                  PacketInfo* packet_info);
    bool ParseAfb(const rtcp::CommonHeader& rtcp_block,
                  PacketInfo* packet_info);
    bool ParseBye(const rtcp::CommonHeader& rtcp_block);
    bool ParseXr(const rtcp::CommonHeader& rtcp_block, 
                 PacketInfo* packet_info);

    void HandleReportBlock(const rtcp::ReportBlock& report_block, 
                           PacketInfo* packet_info,
                           uint32_t remote_ssrc);

    // Blocks in Extended Reports
    void HandleXrRrtrBlock(const rtcp::Rrtr& rrtr, uint32_t sender_ssrc);
    void HandleXrDlrrBlock(const rtcp::Dlrr& dlrr);
    void HandleXrTargetBitrateBlock(const rtcp::TargetBitrate& target_birate, 
                                    PacketInfo* packet_info, 
                                    uint32_t ssrc);

    void HandleParseResult(const PacketInfo& packet_info);

    bool IsRegisteredSsrc(uint32_t ssrc) const;

    void RttPeriodicUpdate();

    bool RtcpRrTimeoutLocked(Timestamp now);
    bool RtcpRrSequenceNumberTimeoutLocked(Timestamp now);
private:
    SequenceChecker sequence_checker_;
    Clock* const clock_;
    bool receiver_only_;
    uint32_t remote_ssrc_;
    const TimeDelta report_interval_;
    TimeDelta rtt_;

    int64_t xr_rr_rtt_ms_;
    
    std::unordered_map<int, uint32_t> registered_ssrcs_;
    std::unordered_map<uint32_t, RtcpReportBlock> received_report_blocks_;
    // Round-Trip Time per remote sender ssrc
    std::unordered_map<uint32_t, RttStats> rtts_;
    std::list<RrtrInfo> rrtrs_;
    std::unordered_map<uint32_t, std::list<RrtrInfo>::iterator> rrtr_its_;

    // The last received RTCP sender report.
    RtcpSenderReportStats last_sr_stats_;

    // The last time we received an RTCP Report block
    Timestamp last_time_received_rb_;

    // The time we last received an RTCP RR telling we have successfully
    // delivered RTP packet to the remote side.
    Timestamp last_time_increased_sequence_number_;

    RtcpNackStats nack_stats_;

    size_t num_skipped_packets_;
    int64_t last_skipped_packets_warning_ms_;

    RtcpPacketTypeCounter packet_type_counter_;

    TaskQueueImpl* const work_queue_;
    std::unique_ptr<RepeatingTask> rtt_update_task_ = nullptr;
    
    RtcpPacketTypeCounterObserver* const packet_type_counter_observer_ = nullptr;
    RtcpIntraFrameObserver* const intra_frame_observer_ = nullptr;
    RtcpLossNotificationObserver* const loss_notification_observer_ = nullptr;
    RtcpBandwidthObserver* const bandwidth_observer_ = nullptr;
    RtcpCnameObserver* const cname_observer_ = nullptr;
    RtcpRttObserver* const rtt_observer_ = nullptr;
    RtcpTransportFeedbackObserver* transport_feedback_observer_ = nullptr;
    RtcpNackListObserver* const nack_list_observer_ = nullptr;
    RtcpReportBlocksObserver* const report_blocks_observer_ = nullptr;

};
    
} // namespace naivertc


#endif