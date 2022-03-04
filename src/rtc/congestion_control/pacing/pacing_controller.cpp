#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/time/clock.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis(500);

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

const TimeDelta PacingController::kMaxExpectedQueueLength = TimeDelta::Millis(2000);
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
      media_budget_(DataRate::Zero()),
      padding_budget_(DataRate::Zero()),
      prober_(config.probing_setting),
      packet_queue_(config.include_overhead, last_process_time_) {}

PacingController::~PacingController() {}

void PacingController::SetPacingBitrate(DataRate pacing_bitrate, 
                                        DataRate padding_bitrate) {
    media_bitrate_ = pacing_bitrate;
    padding_bitrate_ = padding_bitrate;
    pacing_bitrate_ = pacing_bitrate;
    padding_budget_.set_target_bitrate(padding_bitrate);

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
    
} // namespace naivertc

