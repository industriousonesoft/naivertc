#ifndef _RTC_RTP_RTCP_RTCP_SENDER_H_
#define _RTC_RTP_RTCP_RTCP_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/base/time_delta.hpp"
#include "rtc/base/timestamp.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/dlrr.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/report_block.hpp"

#include <optional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace naivertc {

class RtcpReceiver;

class RTC_CPP_EXPORT RtcpSender final {
public:
    // Configuration
    struct Configuration {
        // True for a audio version of the RTP/RTCP module object false will create
        // a video version.
        bool audio = false;

        // SSRCs for media and retransmission, respectively.
        // FlexFec SSRC is fetched from |flexfec_sender|.
        uint32_t local_media_ssrc = 0;

        // The clock to use to read time. If nullptr then system clock will be used.
        Clock* clock = nullptr;

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

        // Used when generating TMMBR.
        RtcpReceiver* receiver;
   };
public:
    RtcpSender(Configuration config, std::shared_ptr<TaskQueue> task_queue);

    RtcpSender() = delete;
    RtcpSender(const RtcpSender&) = delete;
    RtcpSender& operator=(const RtcpSender&) = delete;

    ~RtcpSender();

    bool Sending() const;
    void SetSendingStatus(const FeedbackState& feedback_state, bool enable);

    void SetLastRtpTime(uint32_t rtp_timestamp,
                        std::optional<Timestamp> capture_time,
                        std::optional<int8_t> rtp_payload_type);

private:
    // RtcpContext
    class RtcpContext {
    public:
        RtcpContext(const FeedbackState& feedback_state,
                    int32_t nack_size,
                    const uint16_t* nack_list,
                    Timestamp now);

        const FeedbackState& feedback_state_;
        const int32_t nack_size_;
        const uint16_t* nack_list_;
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
        ReportFlag(uint32_t type, bool is_volatile) 
            : type(type), is_volatile(is_volatile) {}

        bool operator<(const ReportFlag& flag) const { return type < flag.type; };
        bool operator=(const ReportFlag& flag) const { return type == flag.type; }
        const uint32_t type;
        const bool is_volatile;
    };

private:
    std::optional<int32_t> ComputeCompoundRtcpPacket(
            const FeedbackState& feedback_state,
            RtcpPacketType rtcp_packt_type,
            int32_t nack_size,
            const uint16_t* nack_list,
            PacketSender& sender);

    void PrepareReport(const FeedbackState& feedback_state);
    std::vector<rtcp::ReportBlock> CreateReportBlocks(const FeedbackState& feedback_state);

    // |duration| being TimeDelta::Zero() means schedule immediately.
    void SetNextRtcpSendEvaluationDuration(TimeDelta duration);

    void SetFlag(uint32_t type, bool is_volatile);
    bool IsFlagPresent(uint32_t type) const;
    bool ConsumeFlag(uint32_t type, bool forced = false);
    bool AllVolatileFlagsConsumed() const;

private:
    const bool audio_;
    uint32_t ssrc_;
    Clock* clock_;
    std::shared_ptr<TaskQueue> task_queue_;  

    const TimeDelta report_interval_;
    bool sending_;

    std::set<ReportFlag> report_flags_;

    int8_t last_rtp_payload_type_ = -1;
    uint32_t last_rtp_timestamp_ = 0;
    std::optional<Timestamp> last_frame_capture_time_;
    std::optional<Timestamp> next_time_to_send_rtcp_;

    std::string cname_;

};
    
} // namespace naivert 

#endif