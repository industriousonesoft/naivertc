#include "rtc/congestion_control/pacing/bitrate_prober.hpp"

namespace naivertc {

BitrateProber::BitrateProber(Configuration config) 
    : config_(config) {}

BitrateProber::~BitrateProber() {}
    
} // namespace naivertc
