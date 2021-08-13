#ifndef _RTC_RTP_RTCP_RTCP_SENDER_H_
#define _RTC_RTP_RTCP_RTCP_SENDER_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/base/time_delta.hpp"
#include "common/task_queue.hpp"
#include "rtp_rtcp_defines.hpp"

#include <optional>
#include <memory>
#include <set>

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

        // std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis;

        // Used when generating TMMBR.
        RtcpReceiver* receiver;
   };
public:
    RtcpSender(Configuration config, std::shared_ptr<TaskQueue> task_queue);

    RtcpSender() = delete;
    RtcpSender(const RtcpSender&) = delete;
    RtcpSender& operator=(const RtcpSender&) = delete;

    ~RtcpSender();

private:
    class PacketSender;
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
    void SetFlag(uint32_t type, bool is_volatile);
    bool IsFlagPresent(uint32_t type) const;
    bool ConsumeFlag(uint32_t type, bool forced = false);
    bool AllVolatileFlagsConsumed() const;

private:
    const bool audio_;
    uint32_t ssrc_;
    Clock* clock_;

    std::shared_ptr<TaskQueue> task_queue_;  

    std::set<ReportFlag> report_flags_;
};
    
} // namespace naivert 

#endif