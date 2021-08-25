#ifndef _RTC_RTP_RTCP_RTP_NON_PACED_PACKET_SENDER_H_
#define _RTC_RTP_RTCP_RTP_NON_PACED_PACKET_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender_egress.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT RtpNonPacedPacketPacer : public RtpPacketSender {
public:
    RtpNonPacedPacketPacer(std::shared_ptr<RtpPacketSenderEgress> sender, 
                           std::shared_ptr<SequenceNumberAssigner> packet_sequencer,
                           std::shared_ptr<TaskQueue> task_queue);
    ~RtpNonPacedPacketPacer();

    void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) override;

private:
    void PrepareForSend(std::shared_ptr<RtpPacketToSend> packet);
private:
    uint16_t transport_sequence_number_;
    std::shared_ptr<RtpPacketSenderEgress> sender_;
    std::shared_ptr<SequenceNumberAssigner> packet_sequencer_;
    std::shared_ptr<TaskQueue> task_queue_;
};
    
} // namespace naivertc

#endif