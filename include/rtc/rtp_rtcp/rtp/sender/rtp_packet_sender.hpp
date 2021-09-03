#ifndef _RTC_RTP_RTCP_RTP_SENDER_RTP_PACKET_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_RTP_PACKET_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interface.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender_egress.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_pacer.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketSender {
public:
    RtpPacketSender(const RtpRtcpInterface::Configuration& config, std::shared_ptr<TaskQueue> task_queue);
    ~RtpPacketSender();

    // Generator
    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);

    std::shared_ptr<RtpPacketToSend> AllocatePacket() const;

    // Send
    bool EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets);

    // NACK
    void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt);

    // RTX
    RtxMode rtx_mode() const;
    void set_rtx_mode(RtxMode mode);
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);

    // FEC
    bool fec_enabled() const;
    bool red_enabled() const;
    size_t FecPacketOverhead() const;

private:
    int32_t ResendPacket(uint16_t packet_id);

private:

    class NonPacedPacketSender {
    public:
        NonPacedPacketSender(RtpPacketSender* const rtp_sender_);
        ~NonPacedPacketSender();

        void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets);

    private:
        void PrepareForSend(std::shared_ptr<RtpPacketToSend> packet);
    private:
        uint16_t transport_sequence_number_;
        RtpPacketSender* const rtp_sender_;
    };
    friend class NonPacedPacketSender;

private:
    RtxMode rtx_mode_;

    std::shared_ptr<Clock> clock_;
    std::shared_ptr<FecGenerator> fec_generator_;
    std::shared_ptr<TaskQueue> task_queue_;

    RtpPacketSequencer packet_sequencer_;
    RtpPacketSentHistory packet_history_;
    RtpPacketSenderEgress packet_sender_;
    RtpPacketGenerator packet_generator_;

    NonPacedPacketSender non_paced_sender_;

};
    
} // namespace naivertc


#endif