#include "rtc/transports/rtc_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

RtcTransport::RtcTransport(Configuration config, Certificate* certificate, TaskQueue* task_queue) 
    : config_(std::move(config)),
      certificate_(certificate),
      task_queue_(task_queue) {}

RtcTransport::~RtcTransport() {}

void RtcTransport::Start() {}

void RtcTransport::Stop() {}

// Private methods

    
} // namespace naivertc
