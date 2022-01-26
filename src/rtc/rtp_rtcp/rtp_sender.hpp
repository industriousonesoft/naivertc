#ifndef _RTC_RTP_RTCP_RTP_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpSender : public RtcpNackListObserver,
                                 public RtcpReportBlocksObserver,
                                 public RtpSendFeedbackProvider {
public:
    RtpSender(const RtpConfiguration& config);
    ~RtpSender() override;

    // Generator
    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);
 
    RtpPacketToSend GeneratePacket() const;

    // Send
    bool EnqueuePackets(std::vector<RtpPacketToSend> packets);

    // Rtp header extensions
    bool Register(std::string_view uri, int id);
    bool IsRegistered(RtpExtensionType type);
    void Deregister(std::string_view uri);

    // Sequence number
    bool AssignSequenceNumber(RtpPacketToSend& packet);
    bool AssignSequenceNumbers(ArrayView<RtpPacketToSend> packets);
    
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
    int32_t ResendPacket(uint16_t seq_num);

private:
    SequenceChecker sequence_checker_;
    RtxMode rtx_mode_;
    Clock* const clock_;
    FecGenerator* const fec_generator_;

    std::unique_ptr<RtpPacketHistory> packet_history_;
    std::unique_ptr<RtpPacketEgresser> packet_egresser_;
    std::unique_ptr<RtpPacketGenerator> packet_generator_;
    std::unique_ptr<RtpPacketSender> packet_sender_;

};
    
} // namespace naivertc


#endif