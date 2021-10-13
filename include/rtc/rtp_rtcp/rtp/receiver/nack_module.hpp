#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_NACK_MODULE_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/components/seq_num_utils.hpp"

#include <memory>
#include <set>
#include <map>

namespace naivertc {

// This class is not thread-safety, the caller MUST privode it.
class RTC_CPP_EXPORT NackModule {
public:
    NackModule(std::shared_ptr<Clock> clock, int64_t send_nack_delay_ms);
    ~NackModule();

    int OnReceivedPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered = false);

    void ClearUpTo(uint16_t seq_num);
    void UpdateRtt(int64_t rtt_ms);

private:
    struct NackInfo {
        NackInfo();
        NackInfo(uint16_t seq_num, 
                 int64_t created_time);

        uint16_t seq_num;
        int64_t created_time;
        std::optional<int64_t> sent_time;
        int retries;
    };

    // Which fields to consider when deciding which packet to nack in batch
    enum NackFilterType { 
        SEQ_NUM,
        TIME
    };
private:
    void AddMissingPackets(uint16_t seq_num_start, uint16_t seq_num_end);
    bool RemovePacketsUntilKeyFrame();
    std::vector<uint16_t> GetNackBatch(NackFilterType type);
private:
    std::shared_ptr<Clock> clock_;

    bool intialized_;
    int64_t rtt_ms_;
    uint16_t newest_seq_num_;
    // Delay before send nack on packet received.
    const int64_t send_nack_delay_ms_;

    // FIXME: Why not use AscendingComp here?
    std::set<uint16_t, seq_num_utils::DescendingComp<uint16_t>> keyframe_list_;
    std::set<uint16_t, seq_num_utils::DescendingComp<uint16_t>> recovered_list_;
    std::map<uint16_t, NackInfo, seq_num_utils::DescendingComp<uint16_t>> nack_list_;
};
    
} // namespace naivertc


#endif