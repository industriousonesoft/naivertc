#ifndef _RTC_RTP_RTCP_RTP_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/transports/base_transport.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

class RtpSender : public RtcpNackListObserver,
                  public RtcpReportBlocksObserver,
                  public RtpSendStatsProvider {
public:
    RtpSender(const RtpConfiguration& config);
    ~RtpSender() override;

    uint32_t timestamp_offset() const;

    // Generator
    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);
 
    RtpPacketToSend GeneratePacket() const;

    // Enqueue
    bool EnqueuePacket(RtpPacketToSend packet);
    bool EnqueuePackets(std::vector<RtpPacketToSend> packets);

    // Rtp header extensions
    bool Register(std::string_view uri, int id);
    bool IsRegistered(RtpExtensionType type);
    void Deregister(std::string_view uri);

    // Change the first sequence number of |RtpPacketSequencer|
    void SetSequenceNumberOffset(uint16_t seq_num);

    // Store the sent packets, needed to answer to Negative acknowledgment requests.
    void SetStorePacketsStatus(const bool enable, const uint16_t number_to_store);

    // RTX
    int rtx_mode() const;
    void set_rtx_mode(int mode);
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);

    // FEC
    bool fec_enabled() const;
    bool red_enabled() const;
    size_t FecPacketOverhead() const;
    std::vector<RtpPacketToSend> FetchFecPackets() const;

    // Padding
    std::vector<RtpPacketToSend> GeneratePadding(size_t target_packet_size, 
                                                 bool media_has_been_sent,
                                                 bool can_send_padding_on_media_ssrc);

    // Implements RtcpNackListObserver
    void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t rrt_ms) override;

    // Implements RtcpReportBlocksObserver
    void OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks) override;

    // Implements RtpSendStatsProvider
    RtpSendStats GetSendStats() override;

private:
    int32_t ResendPacket(uint16_t seq_num);

private:
    // RtpSenderContext
    struct RtpSenderContext {
        RtpSenderContext(const RtpConfiguration& config);

        RtpPacketSequencer packet_sequencer;
        RtpPacketHistory packet_history;
        RtpPacketGenerator packet_generator;
        RtpPacketEgresser packet_egresser;
        RtpPacketEgresser::NonPacedPacketSender non_paced_sender;
    };
private:
    SequenceChecker sequence_checker_;
    Clock* const clock_;

    std::unique_ptr<RtpSenderContext> ctx_;

    FecGenerator* const fec_generator_;
    RtpPacketSender* const paced_sender_;

    uint32_t timestamp_offset_ = 0;
};
    
} // namespace naivertc


#endif