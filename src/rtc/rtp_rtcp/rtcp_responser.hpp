#ifndef _RTC_RTP_RTCP_RTCP_RESPONSER_H_
#define _RTC_RTP_RTCP_RTCP_RESPONSER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtcpResponser : public KeyFrameRequestSender,
                                     public RtcpReceiveFeedbackProvider {
public:
    RtcpResponser(const RtcpConfiguration& config);
    ~RtcpResponser() override;

    TimeDelta rtt() const;
    
    void set_remote_ssrc(uint32_t remote_ssrc);
    void set_sending(bool enable);

    RtcpMode rtcp_mode() const;
    void set_rtcp_mode(RtcpMode mode);
    
    void RegisterPayloadFrequency(int payload_type,
                                  int payload_frequency);
    

    void IncomingRtcpPacket(const uint8_t* packet, size_t packet_size);
    void IncomingRtcpPacket(CopyOnWriteBuffer rtcp_packet);

    bool SendNack(const std::vector<uint16_t>& nack_list);

    int64_t ExpectedRestransmissionTimeMs() const;

    std::optional<RttStats> GetRttStats(uint32_t ssrc) const;

    bool OnReadyToSendRtpFrame(uint32_t timestamp,
                               int64_t capture_time_ms,
                               int payload_type,
                               bool send_sr_before_key_frame);

    // KeyFrameRequestSender override methods
    void RequestKeyFrame() override;

    // Implements RtcpReceiveFeedbackProvider
    RtcpReceiveFeedback GetReceiveFeedback() override;

private:
    bool TimeToSendFullNackList(int64_t now_ms) const;

private:
    SequenceChecker sequence_checker_;
    Clock* const clock_;

    RtcpSender rtcp_sender_;
    RtcpReceiver rtcp_receiver_;

    int64_t nack_last_time_sent_full_ms_;
    uint16_t nack_last_seq_num_sent_;

};
    
} // namespace naivertc


#endif