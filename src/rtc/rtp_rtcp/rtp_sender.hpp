#ifndef _RTC_RTP_RTCP_RTP_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_pacer.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {
class RTC_CPP_EXPORT RtpSender : public RtcpNackListObserver,
                                 public RtcpReportBlocksObserver,
                                 public RtpSendFeedbackProvider {
public:
    RtpSender(const RtpConfiguration& config, 
              std::unique_ptr<FecGenerator> fec_generator);
    virtual ~RtpSender() override;

    // Generator
    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);
 
    RtpPacketToSend AllocatePacket() const;

    // Send
    bool EnqueuePackets(std::vector<RtpPacketToSend> packets);
    
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

    // Implements RtcpNackListObserver
    void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t rrt_ms) override;

    // Implements RtcpReportBlocksObserver
    void OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                       int64_t rtt_ms) override;

    // Implements RtpSendFeedbackProvider
    RtpSendFeedback GetSendFeedback() override;

private:
    int32_t ResendPacket(uint16_t packet_id);

private:
    // NonPacedPacketSender
    class NonPacedPacketSender {
    public:
        NonPacedPacketSender(RtpSender* const sender);
        ~NonPacedPacketSender();

        void EnqueuePackets(std::vector<RtpPacketToSend> packets);

    private:
        void PrepareForSend(RtpPacketToSend& packet);
    private:
        uint16_t transport_sequence_number_;
        RtpSender* const sender_;
    };
    friend class NonPacedPacketSender;
private:
    SequenceChecker sequence_checker_;
    RtxMode rtx_mode_;
    Clock* const clock_;
    std::unique_ptr<FecGenerator> fec_generator_;

    RtpPacketSequencer packet_sequencer_;
    RtpPacketHistory packet_history_;
    RtpPacketEgresser packet_egresser_;
    RtpPacketGenerator packet_generator_;
    NonPacedPacketSender non_paced_sender_;

};
    
} // namespace naivertc


#endif