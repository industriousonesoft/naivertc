#include "rtc/rtp_rtcp/rtp/receiver/nack_module_impl.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr size_t kMaxPacketAge = 10000;
constexpr size_t kMaxNackPacketCount = 1000;
constexpr int kDefaultRttMs = 100;
constexpr int kMaxNackRetries = 10;

} // namespace

// NackInfo
NackModuleImpl::NackInfo::NackInfo() 
    : seq_num(0), 
      created_time(-1), 
      sent_time(std::nullopt), 
      retries(0) {}

NackModuleImpl::NackInfo::NackInfo(uint16_t seq_num, 
                                   int64_t created_time) 
    : seq_num(seq_num), 
      created_time(created_time), 
      sent_time(std::nullopt), 
      retries(0) {}

// NackModuleImpl
NackModuleImpl::NackModuleImpl(Clock* clock, 
                               int64_t send_nack_delay_ms) 
    : clock_(clock),
      send_nack_delay_ms_(send_nack_delay_ms),
      initialized_(false),
      rtt_ms_(kDefaultRttMs),
      newest_seq_num_(0) {}

NackModuleImpl::~NackModuleImpl() {}

void NackModuleImpl::ClearUpTo(uint16_t seq_num) {
    nack_list_.erase(nack_list_.begin(), nack_list_.lower_bound(seq_num));
    keyframe_list_.erase(keyframe_list_.begin(), keyframe_list_.lower_bound(seq_num));
    recovered_list_.erase(recovered_list_.begin(), recovered_list_.lower_bound(seq_num));
}

void NackModuleImpl::UpdateRtt(int64_t rtt_ms) {
    rtt_ms_ = rtt_ms;
}

NackModuleImpl::InsertResult NackModuleImpl::InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered) {
    InsertResult ret;
    if (!initialized_) {
        newest_seq_num_ = seq_num;
        if (is_keyframe) {
            keyframe_list_.insert(seq_num);
        }
        initialized_ = true;
        return ret;
    }

    if (seq_num == newest_seq_num_) {
        return ret;
    }

    // `seq_num` is newer than `newest_seq_num`
    if (wrap_around_utils::AheadOf(newest_seq_num_, seq_num)) {
        size_t nacks_sent_for_packet = 0;
        auto it = nack_list_.find(seq_num);
        if (it != nack_list_.end()) {
            nacks_sent_for_packet = it->second.retries;
            nack_list_.erase(it);
        }
        ret.nacks_sent_for_seq_num = nacks_sent_for_packet;
        return ret;
    }

    // Keep track of new keyframe.
    if (is_keyframe) {
        keyframe_list_.insert(seq_num);
    }
    // Remove old ones so we don't accumulate keyframes.
    auto it = keyframe_list_.lower_bound(seq_num - kMaxPacketAge);
    if (it != keyframe_list_.begin()) {
        keyframe_list_.erase(keyframe_list_.begin(), it);
    }

    // Update recovered packet list.
    if (is_recovered) {
        recovered_list_.insert(seq_num);
        // Remove old ones so we don't accumulate recovered packets.
        auto it = recovered_list_.lower_bound(seq_num - kMaxPacketAge);
        if (it != recovered_list_.begin()) {
            recovered_list_.erase(recovered_list_.begin(), it);
        }
        // Don't send nack for packets recovered by FEC or RTX.
        return ret;
    }

    // Add missing packets: [newest_seq_num_ + 1, seq_num - 1];
    // False on nack list is cleared as overflow, and requesting a keyframe.
    ret.keyframe_requested = !AddPacketsToNack(newest_seq_num_ + 1, seq_num);
    newest_seq_num_ = seq_num;

    // Are there any nacks that are waiting for `newest_seq_num_`.
    ret.nack_list_to_send = NackListUpTo(newest_seq_num_);

    return ret;
}

std::vector<uint16_t> NackModuleImpl::NackListUpTo(uint16_t seq_num) {
     // Are there any nacks that are waiting for this seq_num.
    return NackListToSend(NackModuleImpl::NackFilterType::SEQ_NUM, seq_num);
}

std::vector<uint16_t> NackModuleImpl::NackListOnRttPassed() {
    // Are there any nacks that are waiting to send.
    return NackListToSend(NackModuleImpl::NackFilterType::TIME, newest_seq_num_);
}

// Private methods
bool NackModuleImpl::AddPacketsToNack(uint16_t seq_num_start, uint16_t seq_num_end) {
    auto it = nack_list_.lower_bound(seq_num_end - kMaxPacketAge);
    nack_list_.erase(nack_list_.begin(), it);

    uint16_t num_new_nacks = ForwardDiff(seq_num_start, seq_num_end);
    if (nack_list_.size() + num_new_nacks > kMaxNackPacketCount) {
        while (RemovePacketsUntilKeyFrame() && 
               nack_list_.size() + num_new_nacks > kMaxNackPacketCount) {}

        if (nack_list_.size() + num_new_nacks > kMaxNackPacketCount) {
            PLOG_WARNING << "NACK list is full, clearing it and requesting a keyframe.";
            nack_list_.clear();
            return false;
        }
    }

    for (uint16_t seq_num = seq_num_start; seq_num != seq_num_end; ++seq_num) {
        // Don't send nack for packets recovered by FEC or RTX.
        if (recovered_list_.find(seq_num) != recovered_list_.end()) {
            continue;
        }
        NackInfo nack_info(seq_num, clock_->now_ms());
        nack_list_[seq_num] = std::move(nack_info);
    }   
    return true;
}

bool NackModuleImpl::RemovePacketsUntilKeyFrame() {
    while (!keyframe_list_.empty()) {
        auto it = nack_list_.lower_bound(*keyframe_list_.begin());
        if (it != nack_list_.begin()) {
            // We have found a keyframe that actually is newer than at least one
            // packet in the nack list.
            nack_list_.erase(nack_list_.begin(), it);
            return true;
        }
        // If this keyframe is so old it does not remove any packets from the list,
        // remove it from the list of keyframes and try the next keyframe.
        keyframe_list_.erase(keyframe_list_.begin());
    }
    return false;
}

std::vector<uint16_t> NackModuleImpl::NackListToSend(NackFilterType type, uint16_t seq_num) {
    Timestamp now = clock_->CurrentTime();
    std::vector<uint16_t> nack_list_to_send;
    auto it = nack_list_.begin();
    while (it != nack_list_.end()) {
        TimeDelta resend_delay = TimeDelta::Millis(rtt_ms_);
        // Delay to send nack timed out.
        bool delay_timed_out = now.ms() - it->second.created_time >= send_nack_delay_ms_;
        if (delay_timed_out) {
            bool nack_on_rtt_passed = false;
            // Nack on rtt passed and sent once.
            if (type == NackFilterType::TIME && it->second.sent_time) {
                nack_on_rtt_passed = now.ms() - *it->second.sent_time >= resend_delay.ms();
            }
            bool nack_on_seq_num_passed = false;
            // Nack on seq_num passed and not sent before.
            if (type == NackFilterType::SEQ_NUM && !it->second.sent_time) {
                nack_on_seq_num_passed = wrap_around_utils::AheadOrAt(seq_num, it->second.seq_num);
            }

            if (nack_on_rtt_passed || nack_on_seq_num_passed) {
                nack_list_to_send.emplace_back(it->second.seq_num);
                ++it->second.retries;
                it->second.sent_time = now.ms();
                if (it->second.retries >= kMaxNackRetries) {
                    PLOG_WARNING << "Sequence number " << it->second.seq_num
                                 << " remove from NACK list due to max retries.";
                    it = nack_list_.erase(it);
                } else {
                    ++it;
                }
                continue;
            }
        }
        ++it;
    }
    return nack_list_to_send;
}

} // namespace naivertc