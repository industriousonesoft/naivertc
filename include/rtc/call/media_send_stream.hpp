#ifndef _RTC_CALL_SEND_STREAM_BASE_H_
#define _RTC_CALL_SEND_STREAM_BASE_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/call/rtp_config.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

namespace naivertc {

class RTC_CPP_EXPORT MediaSendStream : public RtcpReceiver::Observer {
public:
    enum class MediaType {
        VIDEO,
        AUDIO
    };  
public:
    MediaSendStream(const RtpConfig& rtp_config,
                    std::shared_ptr<Clock> clock,
                    std::shared_ptr<Transport> send_transport, 
                    std::shared_ptr<TaskQueue> task_queue);
    virtual ~MediaSendStream();
    virtual MediaType media_type() const;

private:
    void InitRtpRtcpModules(const RtpConfig& rtp_config,
                            std::shared_ptr<Clock> clock,
                            std::shared_ptr<Transport> send_transport,
                            std::shared_ptr<TaskQueue> task_queue);

    std::shared_ptr<FecGenerator> MaybeCreateFecGenerator(const RtpConfig& rtp_config, uint32_t media_ssrc);

private:
    // Rtcp sender
    RtcpSender::FeedbackState GetFeedbackState();
    void MaybeSendRtcp();
    void ScheduleRtcpSendEvaluation(TimeDelta duration);
    void MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time);
    
    // Rtcp Receiver Observer
    void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) override;
    void OnRequestSendReport() override;
    void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) override;
    void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) override; 
private:
    const RtpConfig rtp_config_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    TaskQueue rtcp_task_queue_;
    
    std::shared_ptr<RtcpSender> rtcp_sender_ = nullptr;
    std::shared_ptr<RtcpReceiver> rtcp_receiver_ = nullptr;
    std::shared_ptr<RtpSender> rtp_sender_ = nullptr;
    std::shared_ptr<FecGenerator> fec_generator_ = nullptr;
};
    
} // namespace naivertc


#endif