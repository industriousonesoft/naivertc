#ifndef _RTC_RTP_RTCP_RTP_SENDER_RTP_PACKET_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_RTP_PACKET_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_pacer.hpp"

namespace naivertc {
class RTC_CPP_EXPORT RtpSender {
public:
    RtpSender(const RtpConfiguration& config, 
              std::unique_ptr<FecGenerator> fec_generator, 
              std::shared_ptr<TaskQueue> task_queue);
    virtual ~RtpSender();

    // Generator
    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);

    std::shared_ptr<RtpPacketToSend> AllocatePacket() const;

    // Send
    bool EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets);

    // NACK
    void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt);
    // Store the sent packets, needed to answer to Negative acknowledgment requests.
    void SetStorePacketsStatus(const bool enable, const uint16_t number_to_store);

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
    // NonPacedPacketSender
    class NonPacedPacketSender {
    public:
        NonPacedPacketSender(RtpSender* const sender);
        ~NonPacedPacketSender();

        void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets);

    private:
        void PrepareForSend(std::shared_ptr<RtpPacketToSend> packet);
    private:
        uint16_t transport_sequence_number_;
        RtpSender* const sender_;
    };
    friend class NonPacedPacketSender;
private:
    RtxMode rtx_mode_;

    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::unique_ptr<FecGenerator> fec_generator_;

    RtpPacketSequencer packet_sequencer_;
    RtpPacketSentHistory packet_history_;
    RtpPacketEgresser packet_egresser_;
    RtpPacketGenerator packet_generator_;
    NonPacedPacketSender non_paced_sender_;

};
    
} // namespace naivertc


#endif