#ifndef _RTC_CALL_SEND_STREAM_BASE_H_
#define _RTC_CALL_SEND_STREAM_BASE_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/call/rtp_rtcp_config.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/rtcp_module.hpp"
#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpMediaSender {
public:
    enum class MediaType {
        VIDEO,
        AUDIO
    };  
public:
    RtpMediaSender(RtpRtcpConfig rtp_rtcp_config,
                  Clock* clock,
                  Transport* send_transport);
    virtual ~RtpMediaSender();
    virtual MediaType media_type() const;

private:
    void InitRtpRtcpModules(const RtpRtcpConfig& rtp_rtcp_config,
                            Clock* clock,
                            Transport* send_transport);

    std::unique_ptr<FecGenerator> MaybeCreateFecGenerator(const RtpRtcpConfig& rtp_config, uint32_t media_ssrc);

protected:
    SequenceChecker sequence_checker_;
    const RtpRtcpConfig rtp_rtcp_config_;
    Clock* const clock_;
    std::unique_ptr<RtcpModule> rtcp_module_;
    std::unique_ptr<RtpSender> rtp_sender_;
};
    
} // namespace naivertc


#endif