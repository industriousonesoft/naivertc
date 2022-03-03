#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/time/clock.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr int kDefaultPriority = 0;
  
} // namespace


PacingController::PacingController(const Configuration& config) 
    : drain_large_queue_(config.drain_large_queue),
      send_padding_if_silent_(config.send_padding_if_silent),
      pace_audio_(config.pace_audio), 
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

void PacingController::SetCongestionWindow(size_t window_size) {

}

void PacingController::EnqueuePacket(RtpPacketToSend packet) {
    if (pacing_bitrate_ <= DataRate::Zero()) {
        PLOG_WARNING << "The pacing bitrate must be set before enqueuing packet.";
        return;
    }
    const int priority = PriorityForType(packet.packet_type());
    EnqueuePacketInternal(std::move(packet), priority);
}

// Private methods
int PacingController::PriorityForType(RtpPacketType packet_type) {
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

void PacingController::EnqueuePacketInternal(RtpPacketToSend packet, 
                                             const int priority) {
    prober_.OnIncomingPacket(packet.size());

    // if (packet_queue_.)

}
    
} // namespace naivertc

