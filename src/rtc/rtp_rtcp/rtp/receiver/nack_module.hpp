#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/nack_module_impl.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"

namespace naivertc {

static constexpr TimeDelta kDefaultUpdateInterval = TimeDelta::Millis(20);
static constexpr int kDefaultSendNackDelayMs = 0;

class RTC_CPP_EXPORT NackModule final {
public:
    NackModule(std::shared_ptr<Clock> clock,
               std::shared_ptr<TaskQueue> task_queue,
               std::weak_ptr<NackSender> nack_sender,
               std::weak_ptr<KeyFrameRequestSender> key_frame_request_sender,
               int64_t send_nack_delay_ms = kDefaultSendNackDelayMs,
               TimeDelta update_interval = kDefaultUpdateInterval);
    ~NackModule();

    size_t InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered);

    void ClearUpTo(uint16_t seq_num);
    void UpdateRtt(int64_t rtt_ms);
    
private:
    void PeriodicUpdate();
private:
    NackModuleImpl impl_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::weak_ptr<NackSender> nack_sender_;
    std::weak_ptr<KeyFrameRequestSender> key_frame_request_sender_;

    std::unique_ptr<RepeatingTask> periodic_task_;

};
    
} // namespace naivertc


#endif