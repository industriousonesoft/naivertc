#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/time/clock.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis(500);

// The maximum debt level, in terms of time, capped when sending packets.
constexpr TimeDelta kMaxDebtInTime = TimeDelta::Millis(500);
constexpr TimeDelta kMaxElapsedTime = TimeDelta::Seconds(2);

// Allow probes to be processed slightly ahead of inteded send time. Currently
// set to 1 ms as this is intended to allow times be rounded down to the nearest
// millisecond.
constexpr TimeDelta kMaxEarlyProbeProcessing = TimeDelta::Millis(1);

constexpr int kDefaultPriority = 0;

int PriorityForType(RtpPacketType packet_type) {
  // Lower numbers takes priority over higher number.
    switch (packet_type)
    {
    case RtpPacketType::AUDIO:
        // Audio packet is always prioritized over others.
        return kDefaultPriority + 1;
    case RtpPacketType::RETRANSMISSION:
        // Send retransmission packet before new media.
        return kDefaultPriority + 2;
    case RtpPacketType::VIDEO:
    case RtpPacketType::FEC:
        // Video packet has normal priority.
        // Send FEC packet concurrently to video packet, as
        // the FEC packet might have a lower chance of being
        // useful if delayed.
        return kDefaultPriority + 3;
    case RtpPacketType::PADDING:
        // The padding is likey useless, only sent to keep 
        // the bandwidth high.
        return kDefaultPriority;
    }
}
  
} // namespace

const TimeDelta PacingController::kMaxExpectedQueueTime = TimeDelta::Millis(2000);
const float PacingController::kDefaultPaceMultiplier = 2.5f;
const TimeDelta PacingController::kPausedProcessInterval = kCongestedPacketInterval;

PacingController::PacingController(const Configuration& config) 
    : drain_large_queue_(config.drain_large_queue),
      send_padding_if_silent_(config.send_padding_if_silent),
      pacing_audio_(config.pacing_audio), 
      ignore_transport_overhead_(config.ignore_transport_overhead),
      padding_target_duration_(config.padding_target_duration),
      clock_(config.clock),
      packet_sender_(config.packet_sender),
      last_process_time_(clock_->CurrentTime()),
      last_send_time_(last_process_time_),
      prober_(config.probing_setting),
      packet_queue_(last_process_time_),
      queue_time_cap_(kMaxExpectedQueueTime) {}

PacingController::~PacingController() {}

bool PacingController::include_overhead() const {
    return packet_queue_.include_overhead();
}

void PacingController::set_include_overhead() {
    packet_queue_.set_include_overhead();
}

size_t PacingController::transport_overhead() const {
    return packet_queue_.transport_overhead();
}

void PacingController::set_transport_overhead(size_t overhead_per_packet) {
    if (ignore_transport_overhead_) {
        return;
    }
    packet_queue_.set_transport_overhead(overhead_per_packet);
}

void PacingController::SetPacingBitrate(DataRate pacing_bitrate, 
                                        DataRate padding_bitrate) {
    media_bitrate_ = pacing_bitrate;
    padding_bitrate_ = padding_bitrate;
    pacing_bitrate_ = pacing_bitrate;

    PLOG_VERBOSE << "Set pacing bitrate=" << pacing_bitrate.bps() 
                 << " bps, padding bitrate=" << padding_bitrate.bps()
                 << " bps.";
}

void PacingController::SetCongestionWindow(size_t congestion_window_size) {
    const bool was_congested = IsCongested();
    congestion_window_size_ = congestion_window_size;
    if (was_congested && !IsCongested()) {
        // TODO：Update budget
    }
}

void PacingController::OnInflightBytes(size_t inflight_bytes) {
    const bool was_congested = IsCongested();
    inflight_bytes_ = inflight_bytes;
    if (was_congested && !IsCongested()) {
        // TODO：Update budget
    }
}

void PacingController::EnqueuePacket(RtpPacketToSend packet) {
    if (pacing_bitrate_ <= DataRate::Zero()) {
        PLOG_WARNING << "The pacing bitrate must be set before enqueuing packet.";
        return;
    }
    const int priority = PriorityForType(packet.packet_type());
    EnqueuePacketInternal(std::move(packet), priority);
}

void PacingController::ProcessPackets() {
    auto now = clock_->CurrentTime();
    auto target_send_time = NextSendTime();
    TimeDelta early_execute_margin = prober_.IsProbing() ? kMaxEarlyProbeProcessing : TimeDelta::Zero();

    if (target_send_time.IsMinusInfinity()) {
        target_send_time = now;
    } else if (now < target_send_time - early_execute_margin) {
        // We are too early, but if queue is empty still allow draining some debt.
        auto elapsed_time = UpdateProcessTime(now);
        ReduceDebt(elapsed_time);
        return;
    }

    if (target_send_time < last_process_time_) {
        PLOG_WARNING << "The next sent time is older than the last process time.";
        // FIXME: When dose this situation happen?
        ReduceDebt(last_process_time_ - target_send_time);
        target_send_time = last_process_time_;
    }

    auto prev_process_time = last_process_time_;
    TimeDelta elapsed_time = UpdateProcessTime(now);

    // Check if it's time to send a heartbeat packet.
    if (IsTimeToSendHeartbeat(now)) {
        if (packet_counter_ == 0) {
            // We can not send padding until a media packet has first beed sent.
            last_send_time_ = now;
        } else {
            // Generate and send padding packets.
            size_t sent_bytes = 0;
            auto heartbeat_packets = packet_sender_->GeneratePadding(/*size=*/1);
            for (auto& packet : heartbeat_packets) {
                sent_bytes += packet.payload_size() + packet.padding_size();
                packet_sender_->SendPacket(std::move(packet), PacedPacketInfo());
                // FEC protected.
                for (auto& fec_packet : packet_sender_->FetchFecPackets()) {
                    EnqueuePacket(std::move(fec_packet));
                }
            }
            OnPaddingSent(sent_bytes, now);
        }
    }

    if (paused_) {
        return;
    }

    if (elapsed_time > TimeDelta::Zero()) {
        auto target_bitrate = pacing_bitrate_;
        size_t queued_packet_size = packet_queue_.packet_size();
        if (queued_packet_size > 0) {
            packet_queue_.UpdateEnqueueTime(now);
            if (drain_large_queue_) {
                auto avg_time_left = std::max(TimeDelta::Millis(1), queue_time_cap_ - packet_queue_.AverageQueueTime());
                // The minimum bitrate required to drain queue.
                auto min_drain_bitrate_required = queued_packet_size / avg_time_left;
                if (min_drain_bitrate_required > target_bitrate) {
                    target_bitrate = min_drain_bitrate_required;
                    PLOG_WARNING << "Update target bitrate (" << target_bitrate.bps()
                                 << " bps) to drain bitrate (" << min_drain_bitrate_required.bps() << " bps).";
                }
            }
        }
        media_bitrate_ = target_bitrate;
    }

    bool first_packet_in_probe = false;
    PacedPacketInfo pacing_info;
    size_t recommended_probe_size = 0;
    bool is_probing = prober_.IsProbing();
    if (is_probing) {
        // Probe timing is sensitive, and handled explicitly by BitrateProber,
        // so use actual sent time rather |target_send_time|.
        pacing_info.probe_cluster = prober_.NextProbeCluster(now);
        if (pacing_info.probe_cluster.has_value()) {
            first_packet_in_probe = pacing_info.probe_cluster->sent_bytes == 0;
            recommended_probe_size = prober_.RecommendedMinProbeSize();
        } else {
            // No valid probe cluster returned, probe might have timed out.
            is_probing = false;
        }
    }

    size_t sent_bytes = 0;

    while (!paused_) {
        if (first_packet_in_probe) {
            // If it's first packet in probe, we insert a small padding packet so
            // we have a more reliable start window for the rate estimation.
            auto padding_packets = packet_sender_->GeneratePadding(1);
            if (!padding_packets.empty()) {
                // Should return only one padding packet with a requested size of 1 byte.
                assert(padding_packets.size() == 1);
                // Insert padding packet with high prioriy to make sure it won't be
                // preempt by media packets.
                EnqueuePacketInternal(std::move(padding_packets[0]), kDefaultPriority);
            }
            first_packet_in_probe = false;
        }

        if (prev_process_time < target_send_time) {
            // Reduce buffer levels with amount corresponding to time between last
            // process and target send time for the next packet.
            // If the process call is late, that may be the time between the optimal
            // send times for two packets we should already have sent.
            ReduceDebt(target_send_time - prev_process_time);
            prev_process_time = target_send_time;
        }

        std::optional<RtpPacketToSend> rtp_packet = NextPacketToSend(pacing_info, target_send_time, now);
        // No packet available to send.
        if (rtp_packet == std::nullopt) {
            // Check if we should send padding.
            size_t padding_to_add = PaddingToAdd(recommended_probe_size, sent_bytes);
            auto padding_packets = packet_sender_->GeneratePadding(padding_to_add);
            if (!padding_packets.empty()) {
                for (auto& packet : padding_packets) {
                    EnqueuePacket(packet);
                }
                // Continue loop to send the padding that was just added.
                continue;
            }
            // Can't fetch new packet and no padding to send, exit send loop.
            break;
        }

        const RtpPacketType packet_type = rtp_packet->packet_type();
        size_t packet_size = rtp_packet->payload_size() + rtp_packet->padding_size();

        if (include_overhead()) {
            packet_size += rtp_packet->header_size() + transport_overhead();
        }

        packet_sender_->SendPacket(std::move(*rtp_packet), pacing_info);
        for (auto& fec_packet : packet_sender_->FetchFecPackets()) {
            EnqueuePacket(std::move(fec_packet));
        }
        sent_bytes += packet_size;

        OnMediaSent(packet_type, packet_size, target_send_time);

        // Check if we can probing and have reached the reommended 
        // probe size after sending media packet.
        if (is_probing && sent_bytes >= recommended_probe_size) {
            break;
        }

        // FIXME: Why do we update the send time here?
        // Update target send time in case that are more packets 
        // that we are late in processing.
        Timestamp next_sent_time = NextSendTime();
        if (next_sent_time.IsMinusInfinity()) {
            target_send_time = now;
        } else {
            target_send_time = std::min(now, next_sent_time);
        }

    } // end while

    last_process_time_ = std::max(last_process_time_, prev_process_time);

    if (is_probing) {
        probing_send_failure_ = sent_bytes == 0 ? true : false;
        if (!probing_send_failure_) {
            prober_.OnProbeSent(sent_bytes, clock_->CurrentTime());
        }
    }

}

Timestamp PacingController::NextSendTime() const {
    const Timestamp now = clock_->CurrentTime();

    if (paused_) {
        return last_send_time_ + kPausedProcessInterval;
    }

    // If probing is active, that always takes priority.
    if (prober_.IsProbing()) {
        Timestamp probe_time = prober_.NextTimeToProbe(now);
        if (probe_time != Timestamp::PlusInfinity() && !probing_send_failure_) {
            return probe_time;
        }
    }

    if (!pacing_audio_) {
        // when not pacing audio, return the enqueue time if the leading packet
        // is audio.
        auto audio_enqueue_time = packet_queue_.LeadingAudioPacketEnqueueTime();
        if (audio_enqueue_time) {
            return *audio_enqueue_time;
        }
    }

    // In congestion or haven't received any packet so far.
    if (IsCongested() || packet_counter_ == 0) {
        // We need to at least send keep-alive packets with some interval.
        return last_send_time_ + kCongestedPacketInterval;
    }

    // Send media packets first if we can.
    if (media_bitrate_ > DataRate::Zero() && !packet_queue_.Empty()) {
        // The next time we can send next media packet as soon as possible.
        return std::min(last_send_time_ + kPausedProcessInterval, 
                        last_process_time_ + media_debt_ / media_bitrate_);
    }

    // Send padding packet when no packets in queue.
    if (padding_bitrate_ > DataRate::Zero() && packet_queue_.Empty()) {
        // Both media and padding debts should be drained.
        TimeDelta drain_time = std::max(media_debt_ / media_bitrate_, padding_debt_ / padding_bitrate_);
        return std::min(last_send_time_ + kPausedProcessInterval,
                        last_process_time_ + drain_time);
    }

    if (send_padding_if_silent_) {
        return last_send_time_ + kPausedProcessInterval;
    }

    return last_process_time_ + kPausedProcessInterval;
}

bool PacingController::IsCongested() const {
    if (congestion_window_size_ > 0) {
        return inflight_bytes_ >= congestion_window_size_;
    } 
    return false;
}

// Private methods
void PacingController::EnqueuePacketInternal(RtpPacketToSend packet, 
                                             const int priority) {
    prober_.OnIncomingPacket(packet.size());

    auto now = clock_->CurrentTime();
    // Process the incoming packet immediately.
    if (packet_queue_.Empty() && NextSendTime() <= now) {
        // TODO: Update budget
    }
    packet_queue_.Push(priority, clock_->CurrentTime(), packet_counter_++, std::move(packet));
}

TimeDelta PacingController::UpdateProcessTime(Timestamp at_time) {
    // If no previous process or the last process is in the future (as early probe process),
    // then there is no elapsed time to add budget for.
    if (last_process_time_.IsMinusInfinity() || 
        at_time < last_process_time_) {
        return TimeDelta::Zero();
    }

    TimeDelta elapsed_time = at_time - last_process_time_;
    last_process_time_ = at_time;
    if (elapsed_time > kMaxElapsedTime) {
        PLOG_WARNING << "Elapsed time (" << elapsed_time.ms() 
                     << " ms) is longer than expected, limiting to "
                     << kMaxElapsedTime.ms() << " ms.";
        elapsed_time = kMaxElapsedTime;
    }
    return elapsed_time;
}

void PacingController::ReduceDebt(TimeDelta elapsed_time) {
    // Make sure |media_debt_| and |padding_debt_| not becomes a negative.
    media_debt_ -= std::min(media_debt_, media_bitrate_ * elapsed_time);
    padding_debt_ -= std::min(padding_debt_, padding_bitrate_ * elapsed_time);
}

void PacingController::AddDebt(size_t sent_bytes) {
    media_debt_ += sent_bytes;
    padding_debt_ += sent_bytes;
    media_debt_ = std::min(media_debt_, media_bitrate_ * kMaxDebtInTime);
    padding_debt_ = std::min(padding_debt_, padding_bitrate_ * kMaxDebtInTime);
}

bool PacingController::IsTimeToSendHeartbeat(Timestamp at_time) const {
    if (send_padding_if_silent_ || paused_ || IsCongested() || packet_counter_) {
        // We send a padding packet as heartbeat every 500 ms to ensure we won't
        // get stuck in congested state due to no feedback being received.
        auto elapsed_since_last_send = at_time - last_send_time_;
        if (elapsed_since_last_send >= kCongestedPacketInterval) {
            return true;
        }
    }
    return false;
}

void PacingController::OnMediaSent(RtpPacketType packet_type, 
                                   size_t sent_bytes, 
                                   Timestamp at_time) {
    if (!first_sent_packet_time_) {
        first_sent_packet_time_ = at_time;
    }
    if (packet_type == RtpPacketType::AUDIO || account_for_audio_) {
        AddDebt(sent_bytes);
    }
    last_send_time_ = at_time;
    last_process_time_ = at_time;
}

void PacingController::OnPaddingSent(size_t sent_bytes, 
                                     Timestamp at_time) {
    if (sent_bytes > 0) {
        AddDebt(sent_bytes);
    }
    last_send_time_ = at_time;
    last_process_time_ = at_time;
}

std::optional<RtpPacketToSend> PacingController::NextPacketToSend(const PacedPacketInfo& pacing_info,
                                                                  Timestamp target_send_time,
                                                                  Timestamp at_time) {
    if (packet_queue_.Empty()) {
        return std::nullopt;
    }

    // Check if the next packet to send is a unpaced audio packet?
    bool has_unpaced_audio_packet = !pacing_audio_ && packet_queue_.LeadingAudioPacketEnqueueTime().has_value();
    bool is_probing = pacing_info.probe_cluster.has_value();
    // If the next packet is not neither a audio nor used to probe,
    // we need to check it futher.
    if (!has_unpaced_audio_packet && !is_probing) {
        if (IsCongested()) {
            // Don't send any packets (except aduio or probe) if congested.
            return std::nullopt;
        }

        // Allow sending slight early if we could.
        if (at_time <= target_send_time) {
            // The time required to reduce the current debt to zero.
            auto flush_time = media_debt_ / media_bitrate_;
            // Check if we can pay off the debt at the target sent time.
            if (at_time + flush_time > target_send_time) {
                // Wait for next sent time.
                return std::nullopt;
            }
        }
    }
    // The next packet could be audio, probe or others.
    return packet_queue_.Pop();
}

size_t PacingController::PaddingToAdd(size_t recommended_probe_size, size_t sent_bytes) {
    if (!packet_queue_.Empty()) {
        // No need to add padding if we have media packets in queue.
        return 0;
    }
    
    if (IsCongested()) {
        // Don't add padding if congested, even if requested for probing.
        return 0;
    }

    if (packet_counter_ == 0) {
        // Don't add padding until a media packet has first been sent.
        return 0;
    }

    if (recommended_probe_size > 0) {
        if (recommended_probe_size > sent_bytes) {
            return recommended_probe_size - sent_bytes;
        } else {
            return 0;
        }
    }

    if (padding_bitrate_ > DataRate::Zero() && padding_debt_ == 0) {
        return padding_target_duration_ * padding_bitrate_;
    }

    return 0;
}
    
} // namespace naivertc

