#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_IMPL_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/components/seq_num_utils.hpp"

#include <memory>
#include <set>
#include <map>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT NackModuleImpl {
public:
    struct InsertResult {
        // Nacks sent for `seq_num`.
        size_t nacks_sent_for_seq_num = 0;
        // Indicate if the nack list was overflow and cleared, which means 
        // that a key frame request should be sent.
        bool keyframe_requested = false;
        // Nack list on `seq_num` passed.
        std::vector<uint16_t> nack_list_to_send;
    };
public:
    NackModuleImpl(std::shared_ptr<Clock> clock, 
                   int64_t send_nack_delay_ms);
    ~NackModuleImpl();

    InsertResult InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered);

    void ClearUpTo(uint16_t seq_num);
    void UpdateRtt(int64_t rtt_ms);

    std::vector<uint16_t> NackListOnRttPassed();

private:
    struct NackInfo {
        NackInfo();
        NackInfo(uint16_t seq_num, 
                 int64_t created_time);

        uint16_t seq_num;
        int64_t created_time;
        std::optional<int64_t> sent_time;
        size_t retries;
    };

    // Which fields to consider when deciding 
    // which packet to nack in batch
    enum NackFilterType { 
        SEQ_NUM,
        TIME
    };
    
private:
    std::vector<uint16_t> NackListToSend(NackFilterType type, uint16_t seq_num);
    std::vector<uint16_t> NackListUpTo(uint16_t seq_num);
    bool AddPacketsToNack(uint16_t seq_num_start, uint16_t seq_num_end);
    bool RemovePacketsUntilKeyFrame();
private:
    std::shared_ptr<Clock> clock_;
    // Delay before send nack on packet received.
    const int64_t send_nack_delay_ms_;

    bool initialized_;
    int64_t rtt_ms_;
    uint16_t newest_seq_num_;

    // FIXME: Why not use AscendingComp here?
    std::set<uint16_t, seq_num_utils::DescendingComp<uint16_t>> keyframe_list_;
    std::set<uint16_t, seq_num_utils::DescendingComp<uint16_t>> recovered_list_;
    std::map<uint16_t, NackInfo, seq_num_utils::DescendingComp<uint16_t>> nack_list_;
};
    
} // namespace naivertc

#endif