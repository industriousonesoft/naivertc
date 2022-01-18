#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <plog/Log.h>

namespace naivertc {

// PacketState
RtpPacketSentHistory::PacketState::PacketState() = default;
RtpPacketSentHistory::PacketState::PacketState(const PacketState&) = default;
RtpPacketSentHistory::PacketState::~PacketState() = default;

// StoredPacket
RtpPacketSentHistory::StoredPacket::StoredPacket() 
    : send_time_ms_(std::nullopt),
      packet_(std::nullopt),
      pending_transmission_(false),
      insert_order_(0),
      times_retransmitted_(0) {}

RtpPacketSentHistory::StoredPacket::StoredPacket(
    RtpPacketToSend packet,
    std::optional<int64_t> send_time_ms,
    uint64_t insert_order)
    : send_time_ms_(send_time_ms),
      packet_(std::move(packet)),
      // No send time indicates packet is not sent immediately, but instead will
      // be put in the pacer queue and later retrieved via GetPacketAndSetSendTime().
      pending_transmission_(send_time_ms.has_value() == false),
      insert_order_(insert_order),
      times_retransmitted_(0) {}

RtpPacketSentHistory::StoredPacket::StoredPacket(StoredPacket&&) = default;
RtpPacketSentHistory::StoredPacket& RtpPacketSentHistory::StoredPacket::operator=(
    RtpPacketSentHistory::StoredPacket&&) = default;
RtpPacketSentHistory::StoredPacket::~StoredPacket() = default;

void RtpPacketSentHistory::StoredPacket::IncrementTimesRetransmitted(PacketPrioritySet* priority_set) {
    // Check if this StoredPacket is in the priority set. If so, we need to remove
    // it before updating |times_retransmitted_| since that is used in sorting,
    // and then add it back.
    const bool in_priority_set = priority_set && priority_set->erase(this) > 0;
    ++times_retransmitted_;
    if (in_priority_set) {
        auto it = priority_set->insert(this);
        if (!it.second) {
            PLOG_WARNING << "ERROR: Priority set already contains matching packet! In set: insert order = "
                         << (*it.first)->insert_order_
                         << ", times retransmitted = " << (*it.first)->times_retransmitted_
                         << ". Trying to add: insert order = " << insert_order_
                         << ", times retransmitted = " << times_retransmitted_;
        }
        
    }
}

bool RtpPacketSentHistory::StoredPacketCompare::operator()(StoredPacket* lhs,
                                                       StoredPacket* rhs) const {
    // Prefer to send packets we haven't already sent as padding.
    if (lhs->times_retransmitted() != rhs->times_retransmitted()) {
        return lhs->times_retransmitted() < rhs->times_retransmitted();
    }
    // All else being equal, prefer newer packets.
    return lhs->insert_order() > rhs->insert_order();
}

// RtpPacketSentHistory
// Public methods
RtpPacketSentHistory::RtpPacketSentHistory(const RtpConfiguration& config) 
    : clock_(config.clock),
      enable_padding_prio_(config.enable_rtx_padding_prioritization),
      number_to_store_(0),
      mode_(StorageMode::DISABLE),
      rtt_ms_(-1),
      packets_inserted_(0) {}

RtpPacketSentHistory::~RtpPacketSentHistory() {}

void RtpPacketSentHistory::SetStorePacketsStatus(StorageMode mode, size_t number_to_store) {
    RTC_RUN_ON(&sequence_checker_);
     if (number_to_store > kMaxCapacity) {
        PLOG_WARNING << "Number to store is supposed to less than " << kMaxCapacity;
        return;
    }
    if (mode != StorageMode::DISABLE && mode_ != StorageMode::DISABLE) {
        PLOG_WARNING << "Purging packet history in order to re-set status.";
    }
    Reset();
    mode_ = mode;
    number_to_store_ = number_to_store;
}

RtpPacketSentHistory::StorageMode RtpPacketSentHistory::GetStorageMode() const {
    RTC_RUN_ON(&sequence_checker_);
    return mode_;
}

void RtpPacketSentHistory::SetRttMs(int64_t rtt_ms) {
    RTC_RUN_ON(&sequence_checker_);
    if (rtt_ms < 0) {
        PLOG_WARNING << "Invalid RTT: " << rtt_ms;
        return;
    }
    rtt_ms_ = rtt_ms;
    // If storage is not disabled,  packets will be removed after a timeout
    // that depends on the RTT. Changing the RTT may thus cause some packets
    // become "old" and subject to removal.
    if (mode_ != StorageMode::DISABLE) {
        CullOldPackets(clock_->now_ms());
    }
}

void RtpPacketSentHistory::PutRtpPacket(RtpPacketToSend packet, std::optional<int64_t> send_time_ms) {
    RTC_RUN_ON(&sequence_checker_);
    if (packet.empty()) {
        PLOG_WARNING << "Invalid packet to send.";
        return;
    }
    int64_t now_ms = clock_->now_ms();
    if (mode_ == StorageMode::DISABLE) {
        return;
    }

    if (!packet.allow_retransmission()) {
        return;
    }
    CullOldPackets(now_ms);

    // Store packet.
    const uint16_t rtp_seq_no = packet.sequence_number();
    int packet_index = GetPacketIndex(rtp_seq_no);
    if (packet_index >= 0 &&
        static_cast<size_t>(packet_index) < packet_history_.size() &&
        packet_history_[packet_index].packet_) {
        PLOG_WARNING << "Duplicate packet inserted: " << rtp_seq_no;
        // Remove previous packet to avoid inconsistent state.
        RemovePacket(packet_index);
        packet_index = GetPacketIndex(rtp_seq_no);
    }

    // Packet to be inserted ahead of first packet, expand front.
    for (; packet_index < 0; ++packet_index) {
        packet_history_.emplace_front();
    }
    // Packet to be inserted behind last packet, expand back.
    while (static_cast<int>(packet_history_.size()) <= packet_index) {
        packet_history_.emplace_back();
    }

    if (packet_index < 0 || packet_index >= packet_history_.size()) {
        PLOG_WARNING << "Invalid packet index: " << packet_index;
        return;
    }
    if (packet_history_[packet_index].packet_) {
        PLOG_WARNING << "Packet for index :" << packet_index << " is already set.";
        return;
    }
    
    packet_history_[packet_index] = StoredPacket(std::move(packet), send_time_ms, packets_inserted_++);

    if (enable_padding_prio_) {
        if (padding_priority_.size() >= kMaxPaddingtHistory - 1) {
            padding_priority_.erase(std::prev(padding_priority_.end()));
        }
        auto prio_it = padding_priority_.insert(&packet_history_[packet_index]);
        if (!prio_it.second) {
            PLOG_WARNING << "Failed to insert packet into prio set.";
        }
    }
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPacketAndSetSendTime(uint16_t sequence_number) {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return std::nullopt;
    }
    StoredPacket* stored_packet = GetStoredPacket(sequence_number);
    if (stored_packet == nullptr) {
        return std::nullopt;
    }

    int64_t now_ms = clock_->now_ms();
    if (!VerifyRtt(*stored_packet, now_ms)) {
        return std::nullopt;
    } 

    if (stored_packet->send_time_ms_) {
        stored_packet->IncrementTimesRetransmitted(enable_padding_prio_ ? &padding_priority_ : nullptr);
    }
    // Update send-time and mark as no long in pacer queue.
    stored_packet->send_time_ms_ = now_ms;
    stored_packet->pending_transmission_ = false;

    // Return copy of packet instance since it may need to be retransmitted.
    return stored_packet->packet_;
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPacketAndMarkAsPending(uint16_t sequence_number) {
    return GetPacketAndMarkAsPending(sequence_number, [](const RtpPacketToSend& packet){
        return packet;
    });
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPacketAndMarkAsPending(uint16_t sequence_number, 
            std::function<std::optional<RtpPacketToSend>(const RtpPacketToSend&)> encapsulate) {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return std::nullopt;
    }

    StoredPacket* stored_packet = GetStoredPacket(sequence_number);
    if (stored_packet == nullptr || stored_packet->packet_.has_value() == false) {
        return std::nullopt;
    }

    if (stored_packet->pending_transmission_) {
        // Packet already in pacer queue, ignore this request.
        return std::nullopt;
    }

    if (!VerifyRtt(*stored_packet, clock_->now_ms())) {
        // Packet already resent within too short a time window, ignore.
        return std::nullopt;
    }

    // Copy and/or encapsulate packet.
    auto encapsulated_packet = encapsulate(*stored_packet->packet_);
    if (encapsulated_packet) {
        stored_packet->pending_transmission_ = true;
    }

    return encapsulated_packet;
}

void RtpPacketSentHistory::MarkPacketAsSent(uint16_t sequence_number) {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return;
    }

    StoredPacket* stored_packet = GetStoredPacket(sequence_number);
    if (stored_packet == nullptr) {
        return;
    }

    if (!stored_packet->send_time_ms_.has_value()) {
        PLOG_WARNING << "Invalid packet without sent time.";
        return;
    }

    // Update send-time, mark as no longer in pacer queue, and increment
    // transmission count.
    stored_packet->send_time_ms_ = clock_->now_ms();
    stored_packet->pending_transmission_ = false;
    stored_packet->IncrementTimesRetransmitted(enable_padding_prio_ ? &padding_priority_ : nullptr);
}

std::optional<RtpPacketSentHistory::PacketState> RtpPacketSentHistory::GetPacketState(uint16_t sequence_number) const {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return std::nullopt;
    }

    int packet_index = GetPacketIndex(sequence_number);
    if (packet_index < 0 ||
        static_cast<size_t>(packet_index) >= packet_history_.size()) {
        return std::nullopt;
    }
    const StoredPacket& stored_packet = packet_history_[packet_index];
    if (!stored_packet.packet_) {
        return std::nullopt;
    }

    if (!VerifyRtt(stored_packet, clock_->now_ms())) {
        return std::nullopt;
    }

    return StoredPacketToPacketState(stored_packet);
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPayloadPaddingPacket() {
    return GetPayloadPaddingPacket([](const RtpPacketToSend& packet){
        // Just returns a copy of the packet.
        return packet;
    });
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPayloadPaddingPacket(
        std::function<std::optional<RtpPacketToSend>(const RtpPacketToSend&)> encapsulate) {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return std::nullopt;
    }

    StoredPacket* best_packet = nullptr;
    if (enable_padding_prio_ && !padding_priority_.empty()) {
        auto best_packet_it = padding_priority_.begin();
        best_packet = *best_packet_it;
    } else if (!enable_padding_prio_ && !packet_history_.empty()) {
        // Prioritization not available, pick the last packet.
        for (auto it = packet_history_.rbegin(); it != packet_history_.rend(); ++it) {
            if (it->packet_) {
                best_packet = &(*it);
                break;
            }
        }
    }
    if (best_packet == nullptr) {
        return std::nullopt;
    }

    if (best_packet->pending_transmission_) {
        // Because PacedSender releases it's lock when it calls
        // GeneratePadding() there is the potential for a race where a new
        // packet ends up here instead of the regular transmit path. In such a
        // case, just return empty and it will be picked up on the next
        // Process() call.
        return std::nullopt;
    }

    auto padding_packet = encapsulate(*best_packet->packet_);
    if (!padding_packet) {
        return std::nullopt;
    }

    best_packet->send_time_ms_ = clock_->now_ms();
    best_packet->IncrementTimesRetransmitted(enable_padding_prio_ ? &padding_priority_ : nullptr);

    return padding_packet;
}

void RtpPacketSentHistory::CullAcknowledgedPackets(std::vector<const uint16_t> sequence_numbers) {
    RTC_RUN_ON(&sequence_checker_);
    for (uint16_t sequence_number : sequence_numbers) {
        int packet_index = GetPacketIndex(sequence_number);
        if (packet_index < 0 ||
            static_cast<size_t>(packet_index) >= packet_history_.size()) {
                continue;
            }
        RemovePacket(packet_index);
    }
}

bool RtpPacketSentHistory::SetPendingTransmission(uint16_t sequence_number) {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return false;
    }

    StoredPacket* stored_packet = GetStoredPacket(sequence_number);
    if (stored_packet == nullptr) {
        return false;
    }

    stored_packet->pending_transmission_ = true;
    return true;
}

void RtpPacketSentHistory::Clear() {
    RTC_RUN_ON(&sequence_checker_);
    Reset();
}

// Private methods
bool RtpPacketSentHistory::VerifyRtt(const StoredPacket& packet, int64_t now_ms) const {
    if (packet.send_time_ms_.has_value()) {
        // Send-time already set, this check must be for a retransmission
        if (packet.times_retransmitted() > 0 && now_ms < packet.send_time_ms_.value() + rtt_ms_) {
            // This packet has already been retransmitted once, and the time since that even is
            // lower than on RTT. 
            // Ignore request as this packet is likely already in the network pipe.
            return false;
        }
    }
    return true;
}

void RtpPacketSentHistory::Reset() {
    packet_history_.clear();
    padding_priority_.clear();
}

void RtpPacketSentHistory::CullOldPackets(int64_t now_ms) {
    int64_t packet_duration_ms = std::max(kMinPacketDurationRtt * rtt_ms_, kMinPacketDurationMs);
    while (!packet_history_.empty()) {
        if (packet_history_.size() >= kMaxCapacity) {
            // We have reached the absolute max capacity, remove one packet unconditionally
            RemovePacket(0);
            continue;
        }

        const StoredPacket& stored_packet = packet_history_.front();
        if (stored_packet.pending_transmission_) {
            // Don't remove packets in the pacer queue, pending tranmission.
            return;
        }

        if (*stored_packet.send_time_ms_ + packet_duration_ms > now_ms) {
            // Don't cull packets too early to avoid failed retransmission requests.
            return;
        }

        if (packet_history_.size() >= number_to_store_ ||
            *stored_packet.send_time_ms_ + (packet_duration_ms * kPacketCullingDelayFactor) <= now_ms) {
            // Too many packets in history, or this packet has timed out. Remove it
            // and continue.
            RemovePacket(0);
        } else {
            // No more packets can be removed right now.
            return;
        }
    }
    
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::RemovePacket(int packet_index) {
    // Move the packet out from the StoredPacket container.
    std::optional<RtpPacketToSend> rtp_packet = std::nullopt;

    auto& packet_to_remove = packet_history_[packet_index].packet_;
    if (packet_to_remove) {
        rtp_packet = std::move(*packet_to_remove);
        packet_to_remove.reset();
    }
    // Erase from padding priority set, if eligible.
    if (enable_padding_prio_) {
        padding_priority_.erase(&packet_history_[packet_index]);
    }

    if (packet_index == 0) {
        while (!packet_history_.empty() &&
                packet_history_.front().packet_.has_value() == false) {
            packet_history_.pop_front();
        }
    }

    return rtp_packet;
}

int RtpPacketSentHistory::GetPacketIndex(uint16_t sequence_number) const {
    if (packet_history_.empty()) {
        return 0;
    }

    assert(packet_history_.front().packet_);
    int first_seq = packet_history_.front().packet_->sequence_number();
    if (first_seq == sequence_number) {
        return 0;
    }

    int packet_index = sequence_number - first_seq;
    constexpr int kSeqNumSpan = std::numeric_limits<uint16_t>::max() + 1;

    if (wrap_around_utils::AheadOf<uint16_t>(sequence_number, first_seq)) {
        if (sequence_number < first_seq) {
            // Forward wrap.
            packet_index += kSeqNumSpan;
        }
    } else if (sequence_number > first_seq) {
        // Backwards wrap.
        packet_index -= kSeqNumSpan;
    }

    return packet_index;
}

RtpPacketSentHistory::StoredPacket* RtpPacketSentHistory::GetStoredPacket(uint16_t sequence_number) {
    int index = GetPacketIndex(sequence_number);
    if (index < 0 || 
        static_cast<size_t>(index) >= packet_history_.size() ||
        packet_history_[index].packet_.has_value() == false) {
        return nullptr;
    }
    return &packet_history_[index];
}

RtpPacketSentHistory::PacketState RtpPacketSentHistory::StoredPacketToPacketState(const StoredPacket& stored_packet) {
    RtpPacketSentHistory::PacketState state;
    state.rtp_sequence_number = stored_packet.packet_->sequence_number();
    state.send_time_ms = stored_packet.send_time_ms_;
    state.capture_time_ms = stored_packet.packet_->capture_time_ms();
    state.ssrc = stored_packet.packet_->ssrc();
    state.packet_size = stored_packet.packet_->size();
    state.times_retransmitted = stored_packet.times_retransmitted();
    state.pending_transmission = stored_packet.pending_transmission_;
    return state;
}
    
} // namespace naivertc
