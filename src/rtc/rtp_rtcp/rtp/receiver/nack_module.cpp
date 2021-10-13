#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"

#include <plog/Log.h>

namespace naivertc {

constexpr size_t kMaxPacketWindowSize = 1000;
constexpr size_t kMaxNackPacketCount = 1000;
constexpr int kMaxNackRetries = 10;

NackModule::NackInfo::NackInfo() : seq_num(0), created_time(-1), sent_time(std::nullopt), retries(0) {}
NackModule::NackInfo::NackInfo(uint16_t seq_num, 
                   int64_t created_time) 
    : seq_num(seq_num), created_time(created_time), sent_time(std::nullopt), retries(0) {}

NackModule::NackModule(std::shared_ptr<Clock> clock, int64_t send_nack_delay_ms) 
    : clock_(clock),
      send_nack_delay_ms_(send_nack_delay_ms) {}

NackModule::~NackModule() {}

int NackModule::OnReceivedPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered) {
    if (!intialized_) {
        newest_seq_num_ = seq_num;
        if (is_keyframe) {
            keyframe_list_.insert(seq_num);
        }
        intialized_ = true;
        return 0;
    }

    if (seq_num == newest_seq_num_) {
        return 0;
    }

    // `seq_num` is newer than `newest_seq_num`
    if (seq_num_utils::AheadOf(newest_seq_num_, seq_num)) {
        int nacks_sent_for_packet = 0;
        auto it = nack_list_.find(seq_num);
        if (it != nack_list_.end()) {
            nacks_sent_for_packet = it->second.retries;
            nack_list_.erase(it);
        }
        return nacks_sent_for_packet;
    }

    // Keep track of new keyframe
    if (is_keyframe) {
        keyframe_list_.insert(seq_num);
    }

    // Remove old ones so we don't accumulate keyframes.
    auto it = keyframe_list_.lower_bound(seq_num - kMaxPacketWindowSize);
    if (it != keyframe_list_.begin()) {
        keyframe_list_.erase(keyframe_list_.begin(), it);
    }

    if (is_recovered) {
        recovered_list_.insert(seq_num);

        // Remove old ones so we don't accumulate recovered packets.
        auto it = recovered_list_.lower_bound(seq_num - kMaxPacketWindowSize);
        if (it != recovered_list_.begin()) {
            recovered_list_.erase(recovered_list_.begin(), it);
        }

        // Don't send nack for packets recovered by FEC or RTX.
        return 0;
    }

    // Add missing packets: [newest_seq_num_ + 1, seq_num - 1];
    AddMissingPackets(newest_seq_num_ + 1, seq_num);
    newest_seq_num_ = seq_num;

    // Are there any nacks that are waiting for this seq_num.
    auto nack_batch = GetNackBatch(NackFilterType::SEQ_NUM);
    if (!nack_batch.empty()) {
        // TODO: To send nack list
    }

    return 0;
}

void NackModule::ClearUpTo(uint16_t seq_num) {
    nack_list_.erase(nack_list_.begin(), nack_list_.lower_bound(seq_num));
    keyframe_list_.erase(keyframe_list_.begin(), keyframe_list_.lower_bound(seq_num));
    recovered_list_.erase(recovered_list_.begin(), recovered_list_.lower_bound(seq_num));
}

void NackModule::UpdateRtt(int64_t rtt_ms) {
    rtt_ms_ = rtt_ms;
}

// Private methods
void NackModule::AddMissingPackets(uint16_t seq_num_start, uint16_t seq_num_end) {
    auto it = nack_list_.lower_bound(seq_num_end - kMaxPacketWindowSize);
    nack_list_.erase(nack_list_.begin(), it);

    uint16_t num_new_nacks = ForwardDiff(seq_num_start, seq_num_end);
    if (nack_list_.size() + num_new_nacks > kMaxNackPacketCount) {
        while (RemovePacketsUntilKeyFrame() && 
               nack_list_.size() + num_new_nacks > kMaxNackPacketCount) {}
        if (nack_list_.size() + num_new_nacks > kMaxNackPacketCount) {
            PLOG_WARNING << "NACK list is full, clearing it and requesting a keyframe.";
            nack_list_.clear();
            // TODO: Request keyframe.
            return;
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
}

bool NackModule::RemovePacketsUntilKeyFrame() {
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

std::vector<uint16_t> NackModule::GetNackBatch(NackFilterType type) {
    Timestamp now = clock_->CurrentTime();
    std::vector<uint16_t> nack_batch;
    auto it = nack_list_.begin();
    while (it != nack_list_.end()) {
        TimeDelta resend_delay = TimeDelta::Millis(rtt_ms_);
        // Delay to send nack timed out.
        bool ready_to_send = now.ms() - it->second.created_time >= send_nack_delay_ms_;
        if (ready_to_send) {
            // Nack on rtt passed.
            if (type == NackFilterType::TIME && it->second.sent_time.has_value()) {
                ready_to_send = now.ms() - it->second.sent_time.value() >= resend_delay.ms();
            }
            // Nack on seq_num passed.
            if (!ready_to_send && type == NackFilterType::SEQ_NUM) {
                ready_to_send = it->second.sent_time.has_value() == false && seq_num_utils::AheadOrAt(newest_seq_num_, it->second.seq_num);
            }

            if (ready_to_send) {
                nack_batch.emplace_back(it->second.seq_num);
                ++it->second.retries;
                it->second.sent_time = now.ms();
                if (it->second.retries >= kMaxNackRetries) {
                    PLOG_WARNING << "Sequence number " << it->second.seq_num
                                 << " remove from NACK list due to max retries.";
                    it = nack_list_.erase(it);
                }else {
                    ++it;
                }
                continue;
            }
            ++it;
        }
    }
    return nack_batch;
}
    
} // namespace naivertc
