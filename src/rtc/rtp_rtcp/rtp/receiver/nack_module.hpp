#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/nack_module_impl.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"

namespace naivertc {

static constexpr TimeDelta kDefaultUpdateInterval = TimeDelta::Millis(20);
static constexpr int kDefaultSendNackDelayMs = 0;

class NackModule final {
public:
    NackModule(Clock* clock,
               NackSender* nack_sender,
               KeyFrameRequestSender* key_frame_request_sender,
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
    SequenceChecker sequence_checker_;
    NackSender* nack_sender_;
    KeyFrameRequestSender* key_frame_request_sender_;

    std::unique_ptr<RepeatingTask> periodic_task_;
};
    
} // namespace naivertc


#endif