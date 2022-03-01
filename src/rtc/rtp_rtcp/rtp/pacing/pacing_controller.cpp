#include "rtc/rtp_rtcp/rtp/pacing/pacing_controller.hpp"

namespace naivertc {

PacingController::PacingController(const Configuration& config) 
    : drain_large_queue_(config.drain_large_queue),
      send_padding_if_silent_(config.send_padding_if_silent),
      pace_audio_(config.pace_audio), 
      ignore_transport_overhead_(config.ignore_transport_overhead),
      padding_target_duration_(config.padding_target_duration),
      clock_(config.clock),
      packet_sender_(config.packet_sender) {}

PacingController::~PacingController() {}
    
} // namespace naivertc

