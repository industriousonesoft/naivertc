#ifndef _RTC_RTP_RTCP_RTP_RTCP_INTERFACES_H_
#define _RTC_RTP_RTCP_RTP_RTCP_INTERFACES_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_structs.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

#include <vector>

namespace naivertc {

// RtpSentStatisticsObserver
class RTC_CPP_EXPORT RtpSentStatisticsObserver {
public:
    virtual ~RtpSentStatisticsObserver() = default;
    virtual void RtpSentCountersUpdated(const RtpSentCounters& rtp_sent_counters, 
                                        const RtpSentCounters& rtx_sent_counters) = 0;
    virtual void RtpSentBitRateUpdated(const BitRate bit_rate) = 0;
};

// NackSender
class NackSender {
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
class KeyFrameRequestSender {
public:
    virtual ~KeyFrameRequestSender() = default;
    virtual void RequestKeyFrame() = 0;
};

// Callback interface for packets recovered by FlexFEC or ULPFEC. In
// the FlexFEC case, the implementation should be able to demultiplex
// the recovered RTP packets based on SSRC.
class RecoveredPacketReceiver {
public:
    virtual ~RecoveredPacketReceiver() = default;
    virtual void OnRecoveredPacket(CopyOnWriteBuffer packet) = 0;
};
    
} // namespace naivertc


#endif