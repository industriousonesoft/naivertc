#ifndef _RTC_RTP_RTCP_RTCP_SENDER_H_
#define _RTC_RTP_RTCP_RTCP_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/base/time_delta.hpp"
#include "rtc/base/timestamp.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interface.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/dlrr.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/report_block.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/loss_notification.hpp"

#include <optional>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <map>

namespace naivertc {

class RTC_CPP_EXPORT RtcpSender final {
public:
    // Configuration
    struct Configuration {
        // Static helper
        static Configuration FromRtpRtcpConfiguration(const RtpRtcpInterface::Configuration& config);

        // True for a audio version of the RTP/RTCP module object false will create
        // a video version.
        bool audio = false;

        // SSRCs for media and retransmission, respectively.
        // FlexFec SSRC is fetched from |flexfec_sender|.
        uint32_t local_media_ssrc = 0;

        // The clock to use to read time. If nullptr then system clock will be used.
        std::shared_ptr<Clock> clock = nullptr;

        // Optional callback which, if specified, is used by RTCPSender to schedule
        // the next time to evaluate if RTCP should be sent by means of
        // TimeToSendRTCPReport/SendRTCP.
        // The RTCPSender client still needs to call TimeToSendRTCPReport/SendRTCP
        // to actually get RTCP sent.
        std::function<void(TimeDelta)> schedule_next_rtcp_send_evaluation_function;

        std::optional<TimeDelta> rtcp_report_interval;
    };

    // FeedbackState
    struct FeedbackState {
        FeedbackState();
        FeedbackState(const FeedbackState&);
        FeedbackState(FeedbackState&&);

        ~FeedbackState();

        uint32_t packets_sent;
        size_t media_bytes_sent;
        uint32_t send_bitrate;

        uint32_t last_rr_ntp_secs;
        uint32_t last_rr_ntp_frac;
        uint32_t remote_sr;

        std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis;
   };
public:
    RtcpSender(Configuration config, std::shared_ptr<TaskQueue> task_queue);

    RtcpSender() = delete;
    RtcpSender(const RtcpSender&) = delete;
    RtcpSender& operator=(const RtcpSender&) = delete;

    ~RtcpSender();

    uint32_t ssrc() const;
    void set_ssrc(uint32_t ssrc);

    void set_remote_ssrc(uint32_t ssrc);
    void set_cname(std::string cname);
    void set_max_rtp_packet_size(size_t max_packet_size);
    void set_csrcs(const std::vector<uint32_t>& csrcs);

    bool Sending() const;
    void SetSendingStatus(const FeedbackState& feedback_state, bool enable);

    void SetRtpClockRate(int8_t rtp_payload_type, int rtp_clock_rate_hz);

    void SetRemb(uint64_t bitrate_bps, std::vector<uint32_t> ssrcs);

    void SetTimestampOffset(uint32_t timestamp_offset);
    void SetLastRtpTime(uint32_t rtp_timestamp,
                        std::optional<Timestamp> capture_time,
                        std::optional<int8_t> rtp_payload_type);

    bool TimeToSendRtcpReport(bool send_rtcp_before_key_frame = false);
    bool SendRtcp(const FeedbackState& feedback_state,
                  RtcpPacketType packet_type,
                  const std::vector<uint16_t> nackList = {});

    bool SendLossNotification(const FeedbackState& feedback_state,
                              uint16_t last_decoded_seq_num,
                              uint16_t last_received_seq_num,
                              bool decodability_flag,
                              bool buffering_allowed);

private:
    // RtcpContext
    class RtcpContext {
    public:
        RtcpContext(const FeedbackState& feedback_state,
                    const std::vector<uint16_t> nack_list,
                    Timestamp now);

        const FeedbackState& feedback_state_;
        const std::vector<uint16_t> nack_list_;
        const Timestamp now_;
    };

    // PacketSender
    // Helper to put several RTCP packets into lower layer datagram RTCP packet.
    class PacketSender {
    public:
        PacketSender(RtcpPacket::PacketReadyCallback callback, size_t max_packet_size);
        ~PacketSender();

        // Appends a packet to pending compound packet.
        // Sends rtcp packet if buffer is full and resets the buffer.
        void AppendPacket(const RtcpPacket& packet);

        // Sends pending rtcp packet.
        void Send();

    private:
        const RtcpPacket::PacketReadyCallback callback_;
        const size_t max_packet_size_;
        size_t index_ = 0;
        uint8_t buffer_[kIpPacketSize];
    };

    // Report flag
    struct ReportFlag {
        ReportFlag(RtcpPacketType type, bool is_volatile) 
            : type(type), is_volatile(is_volatile) {}

        bool operator<(const ReportFlag& flag) const { return type < flag.type; };
        bool operator=(const ReportFlag& flag) const { return type == flag.type; }
        const RtcpPacketType type;
        const bool is_volatile;
    };

private:
    bool ComputeCompoundRtcpPacket(const FeedbackState& feedback_state,
                                    RtcpPacketType rtcp_packt_type,
                                    const std::vector<uint16_t> nack_list,
                                    PacketSender& sender);

    void PrepareReport(const FeedbackState& feedback_state);
    std::vector<rtcp::ReportBlock> CreateReportBlocks(const FeedbackState& feedback_state);

    void BuildSR(const RtcpContext& context, PacketSender& sender);
    void BuildRR(const RtcpContext& context, PacketSender& sender);
    void BuildSDES(const RtcpContext& context, PacketSender& sender);
    void BuildFIR(const RtcpContext& context, PacketSender& sender);
    void BuildPLI(const RtcpContext& context, PacketSender& sender);
    void BuildREMB(const RtcpContext& context, PacketSender& sender);
    void BuildTMMBR(const RtcpContext& context, PacketSender& sender);
    void BuildTMMBN(const RtcpContext& context, PacketSender& sender);
    void BuildLossNotification(const RtcpContext& context, PacketSender& sender);
    void BuildNACK(const RtcpContext& context, PacketSender& sender);
    void BuildBYE(const RtcpContext& context, PacketSender& sender);

    void InitBuilders();

    // |duration| being TimeDelta::Zero() means schedule immediately.
    void SetNextRtcpSendEvaluationDuration(TimeDelta duration);

    void SetFlag(RtcpPacketType type, bool is_volatile);
    bool IsFlagPresent(RtcpPacketType type) const;
    bool ConsumeFlag(RtcpPacketType type, bool forced = false);
    bool AllVolatileFlagsConsumed() const;

private:
    const bool audio_;
    uint32_t ssrc_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;  

    const TimeDelta report_interval_;
    bool sending_;

    std::set<ReportFlag> report_flags_;
    std::map<int8_t, int> rtp_clock_rates_khz_;
    
    int8_t last_rtp_payload_type_ = -1;
    uint32_t last_rtp_timestamp_ = 0;
    uint32_t timestamp_offset_ = 0;

    std::optional<Timestamp> last_frame_capture_time_;
    std::optional<Timestamp> next_time_to_send_rtcp_;
    
    // SSRC that we receive on our RTP channel
    uint32_t remote_ssrc_ = 0;
    std::string cname_;

    // REMB
    int64_t remb_bitrate_ = 0;
    std::vector<uint32_t> remb_ssrcs_;

    size_t max_packet_size_;

    RtcpNackStats nack_stats_;
    // send CSRCs
    std::vector<uint32_t> csrcs_;

    rtcp::LossNotification loss_notification_;

    typedef void (RtcpSender::*BuilderFunc)(const RtcpContext&, PacketSender&);
    // Map from RTCPPacketType to builder.
    std::map<RtcpPacketType, BuilderFunc> builders_;
};
    
} // namespace naivert 

#endif