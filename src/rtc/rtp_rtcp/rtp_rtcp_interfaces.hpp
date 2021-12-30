#ifndef _RTC_RTP_RTCP_RTP_RTCP_INTERFACES_H_
#define _RTC_RTP_RTCP_RTP_RTCP_INTERFACES_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_statistics.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <vector>

namespace naivertc {

class ReportBlockData;
struct RtcpPacketTypeCounter;

// NackSender
class RTC_CPP_EXPORT NackSender {
public:
    virtual ~NackSender() = default;
    // If |buffering_allowed|, other feedback messages (e.g. key frame requests)
    // may be added to the same outgoing feedback message. In that case, it's up
    // to the user of the interface to ensure that when all buffer-able messages
    // have been added, the feedback message is triggered.
    virtual void SendNack(std::vector<uint16_t> nack_list,
                          bool buffering_allowed) = 0;
};

// KeyFrameRequestSender
class RTC_CPP_EXPORT KeyFrameRequestSender {
public:
    virtual ~KeyFrameRequestSender() = default;
    virtual void RequestKeyFrame() = 0;
};

// RTP observers

// RtpSentStatisticsObserver
class RTC_CPP_EXPORT RtpSentStatisticsObserver {
public:
    virtual ~RtpSentStatisticsObserver() = default;
    virtual void RtpSentCountersUpdated(const RtpSentCounters& rtp_sent_counters, 
                                        const RtpSentCounters& rtx_sent_counters) = 0;
    virtual void RtpSentBitRateUpdated(const DataRate bit_rate) = 0;
};

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

// RTCP observer

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
    virtual void OnReceivedEstimatedBitrate(uint32_t bitrate) = 0;
    virtual void OnReceivedRtcpReceiverReport(const std::vector<ReportBlockData>& report_blocks,
                                              int64_t rtt,
                                              int64_t now_ms) = 0;
};

// RtcpPacketTypeCounterObserver
class RtcpPacketTypeCounterObserver {
public:
    virtual ~RtcpPacketTypeCounterObserver() = default;
    virtual void RtcpPacketTypesCounterUpdated(uint32_t ssrc,
                                               const RtcpPacketTypeCounter& packet_counter) = 0;
};

// TransportFeedbackObserver
class TransportFeedbackObserver {
public:
    virtual ~TransportFeedbackObserver() = default;
    // virtual void OnAddPacket(const RtpPacketSendInfo& packet_info) = 0;
    // virtual void OnTransportFeedback(const rtcp::TransportFeedback& feedback) = 0;
};
    
} // namespace naivertc


#endif