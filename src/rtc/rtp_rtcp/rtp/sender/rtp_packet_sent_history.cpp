#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <plog/Log.h>

namespace naivertc {

// PacketState
RtpPacketSentHistory::PacketState::PacketState() = default;
RtpPacketSentHistory::PacketState::PacketState(const PacketState&) = default;
RtpPacketSentHistory::PacketState::~PacketState() = default;

// StoredPacket
RtpPacketSentHistory::StoredPacket::StoredPacket(
    RtpPacketToSend packet,
    std::optional<int64_t> send_time_ms,
    uint64_t insert_order)
    : send_time_ms(send_time_ms),
      packet(std::move(packet)),
      // No send time indicates packet is not sent immediately, but instead will
      // be put in the pacer queue and later retrieved via GetPacketAndSetSendTime().
      pending_transmission(!send_time_ms),
      num_retransmitted(0),
      insert_order(insert_order) {}

// RtpPacketSentHistory
bool RtpPacketSentHistory::StoredPacket::Compare::operator()(StoredPacket* lhs,
                                                             StoredPacket* rhs) const {
    // Prefer to send packets we haven't already sent as padding.
    if (lhs->num_retransmitted != rhs->num_retransmitted) {
        return lhs->num_retransmitted < rhs->num_retransmitted;
    }
    // All else being equal, prefer newer packets.
    return lhs->insert_order > rhs->insert_order;
}

// RtpPacketSentHistory
// Public methods
RtpPacketSentHistory::RtpPacketSentHistory(Clock* clock, bool enable_padding_prio) 
    : clock_(clock),
      enable_padding_prio_(enable_padding_prio),
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
    // If storage is not disabled, packets will be removed after a timeout
    // that depends on the RTT. 
    // Changing the RTT may thus cause some packets become old and subject to removal.
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
    const uint16_t seq_num = packet.sequence_number();
    int packet_index = GetPacketIndex(seq_num);
    if (packet_index >= 0 &&
        static_cast<size_t>(packet_index) < packet_history_.size() &&
        packet_history_[packet_index]) {
        PLOG_WARNING << "Duplicate packet inserted: " << seq_num;
        // Remove previous packet to avoid inconsistent state.
        RemovePacket(packet_index);
        packet_index = GetPacketIndex(seq_num);
    }

    // Packet to be inserted ahead of first packet, expand front.
    for (; packet_index < 0; ++packet_index) {
        packet_history_.emplace_front();
    }
    // Packet to be inserted behind last packet, expand back.
    while (static_cast<int>(packet_history_.size()) <= packet_index) {
        packet_history_.emplace_back();
    }

    assert(packet_index >= 0);
    assert(packet_index < packet_history_.size());
    assert(packet_history_[packet_index] == std::nullopt);
    
    StoredPacket& stored_packet = packet_history_[packet_index].emplace(StoredPacket(std::move(packet), send_time_ms, packets_inserted_++));

    if (enable_padding_prio_) {
        // Erase the uppest packet in the |padding_priority_| 
        // if there is no space reserved for the new packet.
        if (padding_priority_.size() >= kMaxPaddingtHistory - 1) {
            padding_priority_.erase(std::prev(padding_priority_.end()));
        }
        auto [it, inserted] = padding_priority_.insert(&stored_packet);
        if (!inserted) {
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

    if (!CanBeTransmitted(*stored_packet)) {
        return std::nullopt;
    } 

    // This is a retransmitted packet.
    if (stored_packet->send_time_ms) {
        Retransmitted(*stored_packet);
    }
    // Update send-time and mark as no long in pacer queue.
    stored_packet->send_time_ms = clock_->now_ms();
    stored_packet->pending_transmission = false;

    // Return copy of packet instance since it may need to be retransmitted.
    return stored_packet->packet;
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPacketAndMarkAsPending(uint16_t sequence_number) {
    return GetPacketAndMarkAsPending(sequence_number, [](const RtpPacketToSend& packet){
        return packet;
    });
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPacketAndMarkAsPending(uint16_t sequence_number, 
                                                                               EncapsulateCallback encapsulate) {
    RTC_RUN_ON(&sequence_checker_);
    if (mode_ == StorageMode::DISABLE) {
        return std::nullopt;
    }

    StoredPacket* stored_packet = GetStoredPacket(sequence_number);
    if (stored_packet == nullptr) {
        return std::nullopt;
    }

    if (stored_packet->pending_transmission) {
        // Packet already in pacer queue, ignore this request.
        return std::nullopt;
    }

    if (!CanBeTransmitted(*stored_packet)) {
        // Packet already resent within too short a time window, ignore.
        return std::nullopt;
    }

    // Copy and/or encapsulate packet.
    auto encapsulated_packet = encapsulate != nullptr ? encapsulate(stored_packet->packet) 
                                                      : std::nullopt;
    if (encapsulated_packet) {
        stored_packet->pending_transmission = true;
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

    if (!stored_packet->send_time_ms.has_value()) {
        PLOG_WARNING << "Invalid packet without sent time.";
        return;
    }

    // Update send-time, mark as no longer in pacer queue, and increment
    // transmission count.
    stored_packet->send_time_ms = clock_->now_ms();
    stored_packet->pending_transmission = false;
    Retransmitted(*stored_packet);
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
    const auto& stored_packet = packet_history_[packet_index];
    if (!stored_packet) {
        return std::nullopt;
    }

    // Ignore the non-sendable packet.
    if (!CanBeTransmitted(*stored_packet)) {
        return std::nullopt;
    }

    return StoredPacketToPacketState(*stored_packet);
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPayloadPaddingPacket() {
    return GetPayloadPaddingPacket([](const RtpPacketToSend& packet){
        // Just returns a copy of the packet.
        return packet;
    });
}

std::optional<RtpPacketToSend> RtpPacketSentHistory::GetPayloadPaddingPacket(EncapsulateCallback encapsulate) {
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
        best_packet = GetLastAvailablePacket();
    }
    
    if (best_packet == nullptr) {
        return std::nullopt;
    }
    
    if (best_packet->pending_transmission) {
        // Because PacedSender releases it's lock when it calls
        // GeneratePadding() there is the potential for a race where a new
        // packet ends up here instead of the regular transmit path. In such a
        // case, just return empty and it will be picked up on the next
        // Process() call.
        return std::nullopt;
    }
    
    auto padding_packet = encapsulate != nullptr ? encapsulate(best_packet->packet) 
                                                 : std::nullopt;

    if (!padding_packet) {
        return std::nullopt;
    }

    best_packet->send_time_ms = clock_->now_ms();
    Retransmitted(*best_packet);

    return padding_packet;
}

void RtpPacketSentHistory::CullAckedPackets(ArrayView<const uint16_t> acked_seq_nums) {
    RTC_RUN_ON(&sequence_checker_);
    for (uint16_t acked_seq_num : acked_seq_nums) {
        int packet_index = GetPacketIndex(acked_seq_num);
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

    stored_packet->pending_transmission = true;
    return true;
}

void RtpPacketSentHistory::Clear() {
    RTC_RUN_ON(&sequence_checker_);
    Reset();
}

// Private methods
bool RtpPacketSentHistory::CanBeTransmitted(const StoredPacket& packet) const {
    if (packet.send_time_ms.has_value()) {
        // Check if the packet has too recently been sent.
        if (packet.num_retransmitted > 0 && clock_->now_ms() - *packet.send_time_ms < rtt_ms_) {
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
    int64_t packet_duration_ms = std::max(kMinPacketDurationRttFactor * rtt_ms_, kMinPacketDurationMs);
    while (!packet_history_.empty()) {
        if (packet_history_.size() >= kMaxCapacity) {
            // We have reached the absolute max capacity, remove one packet unconditionally
            RemovePacket(0);
            continue;
        }

        const auto& stored_packet = packet_history_.front();
        if (!stored_packet) {
            continue;
        }

        if (stored_packet->pending_transmission) {
            // Don't remove packets in the pacer queue, pending tranmission.
            return;
        }

        // Don't remove unsent packets.
        if (!stored_packet->send_time_ms) {
            continue;
        }

        if (*stored_packet->send_time_ms + packet_duration_ms > now_ms) {
            // Don't cull packets transmitted too recently
            // to avoid failed retransmission requests.
            return;
        }

        if (packet_history_.size() >= number_to_store_ ||
            IsTimedOut(*stored_packet->send_time_ms, packet_duration_ms, now_ms)) {
            // Remove it and continue: 
            // 1. Too many packets in history.
            // 2. This packet has timed out.
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

    auto& packet_to_remove = packet_history_[packet_index];
    if (packet_to_remove) {
        rtp_packet = std::move(packet_to_remove->packet);
        // Erase from padding priority set, if eligible.
        if (enable_padding_prio_) {
            padding_priority_.erase(&packet_to_remove.value());
        }
        packet_to_remove.reset();
    }

    // Make sure the first entry is always populated.
    if (packet_index == 0) {
        while (!packet_history_.empty() &&
                packet_history_.front() == std::nullopt) {
            packet_history_.pop_front();
        }
    }

    return rtp_packet;
}

int RtpPacketSentHistory::GetPacketIndex(uint16_t sequence_number) const {
    if (packet_history_.empty()) {
        return 0;
    }

    // The first entry is always populated if the |packet_history_| is not empty.
    assert(packet_history_.front() != std::nullopt);
    int first_seq = packet_history_.front()->packet.sequence_number();
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
        packet_history_[index] == std::nullopt) {
        return nullptr;
    }
    return &packet_history_[index].value();
}

RtpPacketSentHistory::StoredPacket* RtpPacketSentHistory::GetLastAvailablePacket() {
    StoredPacket* result = nullptr;
    for (auto it = packet_history_.rbegin(); it != packet_history_.rend(); ++it) {
        if (it->has_value()) {
            result = &(it->value());
            break;
        }
    }
    return result;
}

RtpPacketSentHistory::PacketState RtpPacketSentHistory::StoredPacketToPacketState(const StoredPacket& stored_packet) {
    RtpPacketSentHistory::PacketState state;
    state.rtp_sequence_number = stored_packet.packet.sequence_number();
    state.send_time_ms = stored_packet.send_time_ms;
    state.capture_time_ms = stored_packet.packet.capture_time_ms();
    state.ssrc = stored_packet.packet.ssrc();
    state.packet_size = stored_packet.packet.size();
    state.num_retransmitted = stored_packet.num_retransmitted;
    state.pending_transmission = stored_packet.pending_transmission;
    return state;
}

void RtpPacketSentHistory::Retransmitted(StoredPacket& stored_packet) {
    // Check if this StoredPacket is in the priority set. If so, we need to remove
    // it before updating |num_retransmitted_| since that is used in sorting,
    // and then add it back.
    const bool in_priority_set = enable_padding_prio_ ? padding_priority_.erase(&stored_packet) > 0 
                                                      : false;
    ++stored_packet.num_retransmitted;
    if (in_priority_set) {
        auto [it, inserted] = padding_priority_.insert(&stored_packet);
        if (!inserted) {
            PLOG_WARNING << "ERROR: Priority set already contains matching packet! In set: insert order = "
                         << (*it)->insert_order
                         << ", times retransmitted = " << (*it)->num_retransmitted
                         << ". Trying to add: insert order = " << stored_packet.insert_order
                         << ", times retransmitted = " << stored_packet.num_retransmitted;
        }
    }
}

bool RtpPacketSentHistory::IsTimedOut(int64_t send_time_ms, 
                                      int64_t duration_ms, 
                                      int64_t now_ms) {
    return (send_time_ms + duration_ms * kPacketCullingDelayFactor) <= now_ms;
}
    
} // namespace naivertc
