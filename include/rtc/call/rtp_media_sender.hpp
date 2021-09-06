#ifndef _RTC_CALL_SEND_STREAM_BASE_H_
#define _RTC_CALL_SEND_STREAM_BASE_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/call/rtp_rtcp_config.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_senceiver.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpMediaSender {
public:
    enum class MediaType {
        VIDEO,
        AUDIO
    };  
public:
    RtpMediaSender(const RtpRtcpConfig rtp_rtcp_config,
                   std::shared_ptr<Clock> clock,
                   std::shared_ptr<Transport> send_transport, 
                   std::shared_ptr<TaskQueue> task_queue);
    virtual ~RtpMediaSender();
    virtual MediaType media_type() const;

private:
    void InitRtpRtcpModules(const RtpRtcpConfig& rtp_rtcp_config,
                            std::shared_ptr<Clock> clock,
                            std::shared_ptr<Transport> send_transport,
                            std::shared_ptr<TaskQueue> task_queue);

    std::unique_ptr<FecGenerator> MaybeCreateFecGenerator(const RtpRtcpConfig& rtp_config, uint32_t media_ssrc);

protected:
    std::shared_ptr<RtpSender> rtp_sender_ = nullptr;
private:
    const RtpRtcpConfig rtp_rtcp_config_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    TaskQueue rtcp_task_queue_;
    
    std::unique_ptr<RtcpSenceiver> rtcp_senceiver_ = nullptr;
};
    
} // namespace naivertc


#endif