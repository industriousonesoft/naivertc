#ifndef _RTC_RTP_RTCP_RTCP_SENDER_H_
#define _RTC_RTP_RTCP_RTCP_SENDER_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/dlrr.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/loss_notification.hpp"
#include "rtc/base/task_utils/queued_task.hpp"

#include <optional>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <map>

namespace naivertc {

class RtcpSender final {
public:
    struct Configuration {
        bool audio = false;

        uint32_t local_media_ssrc = 0;

        int rtcp_report_interval_ms = 0;

        Clock* clock;

        RtcMediaTransport* send_transport;

        // Observers
        RtcpPacketTypeCounterObserver* packet_type_counter_observer = nullptr;
        RtcpReportBlockProvider* report_block_provider = nullptr;
        RtpSendStatsProvider* rtp_send_stats_provider = nullptr;
        RtcpReceiveFeedbackProvider* rtcp_receive_feedback_provider = nullptr;
    };
public:
    RtcpSender(Configuration config);

    RtcpSender() = delete;
    RtcpSender(const RtcpSender&) = delete;
    RtcpSender& operator=(const RtcpSender&) = delete;

    ~RtcpSender();

    uint32_t local_ssrc() const;
    uint32_t remote_ssrc() const;
    void set_remote_ssrc(uint32_t remote_ssrc);
 
    void set_cname(std::string cname);
    void set_max_rtp_packet_size(size_t max_packet_size);
    void set_csrcs(std::vector<uint32_t> csrcs);

    bool sending() const;
    void set_sending(bool enable);

    RtcpMode rtcp_mode() const;
    void set_rtcp_mode(RtcpMode mode);

    void SetRtpClockRate(int8_t rtp_payload_type, int rtp_clock_rate_hz);

    void SetRemb(uint64_t bitrate_bps, std::vector<uint32_t> ssrcs);
    void UnsetRemb();

    
    void SetLastRtpTime(uint32_t rtp_timestamp,
                        std::optional<int64_t> capture_time_ms,
                        std::optional<int8_t> rtp_payload_type);

    bool TimeToSendRtcpReport(bool send_rtcp_before_key_frame = false);
    bool SendRtcp(RtcpPacketType packet_type,
                  const uint16_t* nack_list = nullptr,
                  size_t nack_size = 0);

    bool SendLossNotification(uint16_t last_decoded_seq_num,
                              uint16_t last_received_seq_num,
                              bool decodability_flag,
                              bool buffering_allowed); 
private:

    // RtcpContext
    class RtcpContext {
    public:
        RtcpContext(const RtpSendStats* rtp_send_stats,
                    const RtcpSenderReportStats* last_sr_stats,
                    ArrayView<const rtcp::Dlrr::TimeInfo> last_xr_rtis,
                    const uint16_t* nack_list,
                    size_t nack_size,
                    Timestamp now_time);

        const RtpSendStats* rtp_send_stats;
        const RtcpSenderReportStats* last_sr_stats;
        ArrayView<const rtcp::Dlrr::TimeInfo> last_xr_rtis;
        const uint16_t* nack_list;
        size_t nack_size;
        const Timestamp now_time;
    };

    // PacketSender
    // Helper to put several RTCP packets into lower layer datagram RTCP packet.
    class PacketSender {
    public:
        PacketSender(RtcMediaTransport* send_transport,
                     bool is_audio,
                     size_t max_packet_size);
        ~PacketSender();

        size_t max_packet_size() const;
        void set_max_packet_size(size_t max_packet_size);

        // Appends a packet to pending compound packet.
        // Sends rtcp packet if buffer is full and resets the buffer.
        void AppendPacket(const RtcpPacket& packet);

        // Sends pending rtcp packet.
        void Send();

        void Reset();

    private:
        void SendPacket(CopyOnWriteBuffer packet);

    private:
        RtcMediaTransport* const send_transport_;
        const bool is_audio_;
        size_t max_packet_size_;
        size_t index_;
        uint8_t buffer_[kIpPacketSize];
    };

    // Report flag
    struct ReportFlag {
        ReportFlag(RtcpPacketType type, bool is_volatile) 
            : type(type), 
              is_volatile(is_volatile) {}

        bool operator<(const ReportFlag& flag) const { return type < flag.type; };
        bool operator=(const ReportFlag& flag) const { return type == flag.type; }
        const RtcpPacketType type;
        const bool is_volatile;
    };

private:
    bool BuildCompoundRtcpPacket(RtcpPacketType rtcp_packt_type,
                                 const uint16_t* nack_list,
                                 size_t nack_size,
                                 PacketSender& sender);

    void PrepareReport(const RtcpContext& ctx);
    std::vector<rtcp::ReportBlock> CreateReportBlocks(const RtcpSenderReportStats* last_sr_stats);

    void BuildSR(const RtcpContext& ctx, PacketSender& sender);
    void BuildRR(const RtcpContext& ctx, PacketSender& sender);
    void BuildSDES(const RtcpContext& ctx, PacketSender& sender);
    void BuildFIR(const RtcpContext& ctx, PacketSender& sender);
    void BuildPLI(const RtcpContext& ctx, PacketSender& sender);
    void BuildREMB(const RtcpContext& ctx, PacketSender& sender);
    void BuildTMMBR(const RtcpContext& ctx, PacketSender& sender);
    void BuildTMMBN(const RtcpContext& ctx, PacketSender& sender);
    void BuildLossNotification(const RtcpContext& ctx, PacketSender& sender);
    void BuildNACK(const RtcpContext& ctx, PacketSender& sender);
    void BuildBYE(const RtcpContext& ctx, PacketSender& sender);
    void BuildExtendedReports(const RtcpContext& ctx, PacketSender& sender);

    void InitBuilders();

    void SetFlag(RtcpPacketType type, bool is_volatile);
    bool IsFlagPresent(RtcpPacketType type) const;
    bool ConsumeFlag(RtcpPacketType type, bool forced = false);
    bool AllVolatileFlagsConsumed() const;

    // Rtcp send scheduler
    void MaybeSendRtcp();
    void ScheduleForNextRtcpSend(TimeDelta delay);
    void MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time);

private:
    SequenceChecker sequence_checker_;
    const bool audio_;
    uint32_t local_ssrc_;
    // SSRC that we receive on our RTP channel
    uint32_t remote_ssrc_;
    Clock* const clock_;
    RtcpMode rtcp_mode_;
 
    const TimeDelta report_interval_;
    bool sending_;

    std::set<ReportFlag> report_flags_;
    std::map<int8_t, int> rtp_clock_rates_khz_;
    
    int8_t last_rtp_payload_type_ = -1;
    uint32_t last_rtp_timestamp_ = 0;

    std::optional<int64_t> last_frame_capture_time_ms_;
    std::optional<Timestamp> next_time_to_send_rtcp_;
    
    std::string cname_;

    // REMB
    int64_t remb_bitrate_ = 0;
    std::vector<uint32_t> remb_ssrcs_;

    RtcpNackStats nack_stats_;
    // send CSRCs
    std::vector<uint32_t> csrcs_;

    // Full intra request
    uint8_t sequence_number_fir_;

    rtcp::LossNotification loss_notification_;

    PacketSender packet_sender_;

    typedef void (RtcpSender::*BuilderFunc)(const RtcpContext&, PacketSender&);
    // Map from RTCPPacketType to builder.
    std::map<RtcpPacketType, BuilderFunc> builders_;

    RtcpPacketTypeCounter packet_type_counter_;

    RtcpPacketTypeCounterObserver* const packet_type_counter_observer_ = nullptr;
    RtcpReportBlockProvider* const report_block_provider_ = nullptr;
    RtpSendStatsProvider* const rtp_send_stats_provider_ = nullptr;
    RtcpReceiveFeedbackProvider* const rtcp_receive_feedback_provider_ = nullptr;

    TaskQueueImpl* const work_queue_;
    ScopedTaskSafety task_safety_;
};
    
} // namespace naivert 

#endif