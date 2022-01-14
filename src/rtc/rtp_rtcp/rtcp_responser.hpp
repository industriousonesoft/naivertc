#ifndef _RTC_RTP_RTCP_RTCP_SENCEIVER_H_
#define _RTC_RTP_RTCP_RTCP_SENCEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtcpResponser : public NackSender,
                                     public KeyFrameRequestSender,
                                     public RtcpReceiveFeedbackProvider {
public:
    RtcpResponser(const RtcpConfiguration& config);
    ~RtcpResponser() override;

    void set_rtt_ms(int64_t rtt_ms);
    int64_t rtt_ms() const;

    void set_remote_ssrc(uint32_t remote_ssrc);

    void IncomingPacket(const uint8_t* packet, size_t packet_size);
    void IncomingPacket(CopyOnWriteBuffer rtcp_packet);

    // NackSender override methods
    void SendNack(std::vector<uint16_t> nack_list,
                  bool buffering_allowed) override;

    // KeyFrameRequestSender override methods
    void RequestKeyFrame() override;

    int64_t ExpectedRestransmissionTimeMs() const;

    std::optional<RttStats> GetRttStats(uint32_t ssrc) const;

    // Implements RtcpReceiveFeedbackProvider
    RtcpReceiveFeedback GetReceiveFeedback() override;
    
private:
    Clock* const clock_;
    SequenceChecker sequence_checker_;
    
    RtcpSender rtcp_sender_;
    RtcpReceiver rtcp_receiver_;
    TaskQueueImpl* const work_queue_;

    int64_t rtt_ms_;
};
    
} // namespace naivertc


#endif