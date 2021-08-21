#ifndef _RTC_RTP_RTCP_RTP_NON_PACED_PACKET_SENDER_H_
#define _RTC_RTP_RTCP_RTP_NON_PACED_PACKET_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_sender_egress.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_sequencer.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_sender.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT RtpNonPacedPacketSender : public RtpPacketSender {
public:
    RtpNonPacedPacketSender(std::shared_ptr<RtpSenderEgress> sender, std::shared_ptr<RtpPacketSequencer> packet_sequencer);
    ~RtpNonPacedPacketSender();

    void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) override;

private:
    void PrepareForSend(std::shared_ptr<RtpPacketToSend> packet);
private:
    uint16_t transport_sequence_number_;
    std::shared_ptr<RtpSenderEgress> sender_;
    std::shared_ptr<RtpPacketSequencer> packet_sequencer_;
};
    
} // namespace naivertc

#endif