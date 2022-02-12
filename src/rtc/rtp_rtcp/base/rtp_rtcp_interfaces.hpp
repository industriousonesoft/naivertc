#ifndef _RTC_RTP_RTCP_RTP_RTCP_INTERFACES_H_
#define _RTC_RTP_RTCP_RTP_RTCP_INTERFACES_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_structs.hpp"
#include "rtc/rtp_rtcp/base/rtcp_statistic_structs.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <vector>
#include <string>

namespace naivertc {

class RtpPacketToSend;

namespace rtcp {
class TransportFeedback;
class ReportBlock;
}

// RtpPacketSender
class RTC_CPP_EXPORT RtpPacketSender {
public:
    virtual ~RtpPacketSender() = default;
    virtual void EnqueuePackets(std::vector<RtpPacketToSend> packets) = 0;
};

// SequenceNumberAssigner 
// A class that can assign RTP sequence numbers 
// for a packet to be sent.
class RTC_CPP_EXPORT SequenceNumberAssigner {
public:
    virtual ~SequenceNumberAssigner() = default;
    virtual bool AssignSequenceNumber(RtpPacketToSend& packet) = 0;
};

// NackSender
class RTC_CPP_EXPORT NackSender {
public:
    virtual ~NackSender() = default;
    // If |buffering_allowed||buffering_allowed|, other feedback messages (e.g. key frame requests)
    // may be added to the same outgoing feedback message. In that case, it's up
    // to the user of the interface to ensure that when all buffer-able messages
    // have been added, the feedback message is triggered.
    virtual void SendNack(const std::vector<uint16_t>& nack_list,
                          bool buffering_allowed) = 0;
};

// KeyFrameRequestSender
class RTC_CPP_EXPORT KeyFrameRequestSender {
public:
    virtual ~KeyFrameRequestSender() = default;
    virtual void RequestKeyFrame() = 0;
};

// RTP observers

// RecoveredPacketReceiver
// Callback interface for packets recovered by FlexFEC or ULPFEC. In
// the FlexFEC case, the implementation should be able to demultiplex
// the recovered RTP packets based on SSRC.
class RTC_CPP_EXPORT RecoveredPacketReceiver {
public:
    virtual ~RecoveredPacketReceiver() = default;
    virtual void OnRecoveredPacket(CopyOnWriteBuffer packet) = 0;
};

// VideoReceiveStatisticsObserver
class RTC_CPP_EXPORT VideoReceiveStatisticsObserver {
public:
    virtual ~VideoReceiveStatisticsObserver() = default;
    virtual void OnCompleteFrame(bool is_keyframe,
                                 size_t size_bytes) = 0;
    virtual void OnDroppedFrames(uint32_t frames_dropped) = 0;
    virtual void OnFrameBufferTimingsUpdated(int max_decode_ms,
                                             int current_delay_ms,
                                             int target_delay_ms,
                                             int jitter_buffer_ms,
                                             int min_playout_delay_ms,
                                             int render_delay_ms) = 0;

};

// RtpSendDelayObserver
class RtpSendDelayObserver {
public:
    virtual ~RtpSendDelayObserver() = default;
    virtual void OnSendDelayUpdated(int64_t avg_delay_ms,
                                    int64_t max_delay_ms,
                                    int64_t total_delay_ms,
                                    uint32_t ssrc) = 0;
};

// RtpSendBitratesObserver
class RtpSendBitratesObserver {
public:
    virtual ~RtpSendBitratesObserver() = default;
    virtual void OnSendBitratesUpdated(uint32_t total_bitrate_bps,
                                       uint32_t retransmit_bitrate_bps,
                                       uint32_t ssrc) = 0;
};

// RtpSendPacketObserver
class RtpSendPacketObserver {
public:
    virtual ~RtpSendPacketObserver() = default;
    virtual void OnSendPacket(uint16_t packet_id,
                              int64_t capture_time_ms,
                              uint32_t ssrc) = 0;
};

// RtpStreamDataCountersObserver
class RtpStreamDataCountersObserver {
public:
    virtual ~RtpStreamDataCountersObserver() = default;
    virtual void OnStreamDataCountersUpdated(const RtpStreamDataCounters& counters,
                                             uint32_t ssrc) = 0;
};

// RtpTransportFeedbackObserver
class RtpTransportFeedbackObserver {
public:
    virtual ~RtpTransportFeedbackObserver() = default;
    virtual void OnAddPacket(const RtpTransportFeedback& feedback) = 0;
};

// RtpSendFeedbackProvider
class RTC_CPP_EXPORT RtpSendFeedbackProvider {
public:
    virtual ~RtpSendFeedbackProvider() = default;
    virtual RtpSendFeedback GetSendFeedback() = 0;
};

// RTCP observer

// RtcpNackListObserver
class RTC_CPP_EXPORT RtcpNackListObserver {
public:
    virtual ~RtcpNackListObserver() = default;
    virtual void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t rrt_ms) = 0;
};

// RtcpIntraFrameObserver
class RTC_CPP_EXPORT RtcpIntraFrameObserver {
public:
    virtual ~RtcpIntraFrameObserver() = default;
    virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) = 0;
};

// RtcpLossNotificationObserver
class RTC_CPP_EXPORT RtcpLossNotificationObserver {
public:
    virtual ~RtcpLossNotificationObserver() = default;
    virtual void OnReceivedLossNotification(uint32_t ssrc,
                                            uint16_t seq_num_of_last_decodable,
                                            uint16_t seq_num_of_last_received,
                                            bool decodability_flag) = 0;
};

// RtcpBandwidthObserver
class RTC_CPP_EXPORT RtcpBandwidthObserver {
public:
    virtual ~RtcpBandwidthObserver() = default;
    // REMB or TMMBR
    virtual void OnReceivedEstimatedBitrateBps(uint32_t bitrate_bps) = 0;
};

// RtcpPacketTypeCounterObserver
class RTC_CPP_EXPORT RtcpPacketTypeCounterObserver {
public:
    virtual ~RtcpPacketTypeCounterObserver() = default;
    virtual void RtcpPacketTypesCounterUpdated(uint32_t ssrc,
                                               const RtcpPacketTypeCounter& packet_counter) = 0;
};

// RtcpCnameObserver
class RTC_CPP_EXPORT RtcpCnameObserver {
public:
    virtual ~RtcpCnameObserver() = default;
    virtual void OnCname(uint32_t ssrc, std::string_view cname) = 0;
};

// RtcpRttObserver
class RTC_CPP_EXPORT RtcpRttObserver {
public:
    virtual ~RtcpRttObserver() = 0;
    virtual void OnRttUpdated(TimeDelta rtt) = 0;
};

// RtcpTransportFeedbackObserver
class RTC_CPP_EXPORT RtcpTransportFeedbackObserver {
public:
    virtual ~RtcpTransportFeedbackObserver() = default;
    virtual void OnTransportFeedback(const rtcp::TransportFeedback& feedback) = 0;
};

// RtcpReportBlockProvider
class RTC_CPP_EXPORT RtcpReportBlockProvider {
public:
    virtual ~RtcpReportBlockProvider() = default;
    virtual std::vector<rtcp::ReportBlock> GetRtcpReportBlocks(size_t max_blocks) = 0;
};

// RtcpReportBlocksObserver 
class RTC_CPP_EXPORT RtcpReportBlocksObserver {
public:
    virtual ~RtcpReportBlocksObserver() = default;
    virtual void OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                            int64_t rtt_ms) = 0; 
};

// RtcpReceiveFeedbackProvider
class RTC_CPP_EXPORT RtcpReceiveFeedbackProvider {
public:
    virtual ~RtcpReceiveFeedbackProvider() = default;
    virtual RtcpReceiveFeedback GetReceiveFeedback() = 0;
};
    
} // namespace naivertc


#endif