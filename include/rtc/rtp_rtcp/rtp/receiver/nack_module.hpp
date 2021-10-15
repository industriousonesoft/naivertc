#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/repeating_task.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/nack_module_impl.hpp"

namespace naivertc {

static constexpr TimeDelta kDefaultUpdateInterval = TimeDelta::Millis(20);

class RTC_CPP_EXPORT NackModule final {
public:
    NackModule(std::shared_ptr<Clock> clock, 
               int64_t send_nack_delay_ms,
               TimeDelta update_interval,
               std::shared_ptr<TaskQueue> task_queue);
    ~NackModule();

    size_t InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered);

    void ClearUpTo(uint16_t seq_num);
    void UpdateRtt(int64_t rtt_ms);

    using NackListToSendCallback = std::function<void(std::vector<uint16_t> nack_list)>;
    void OnNackListToSend(NackListToSendCallback callback);

    using RequestKeyFrameCallback = std::function<void()>;
    void OnRequestKeyFrame(RequestKeyFrameCallback callback);
    
private:
    void PeriodicUpdate();
private:
    NackModuleImpl impl_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::unique_ptr<RepeatingTask> periodic_task_;
    
    NackListToSendCallback nack_list_to_send_callback_ = nullptr;
    RequestKeyFrameCallback request_key_frame_callback_ = nullptr;
};
    
} // namespace naivertc


#endif