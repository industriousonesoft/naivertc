#include "rtc/media/audio_track.hpp"

namespace naivertc {

AudioTrack::AudioTrack(const Configuration& config) 
    : MediaTrack(config) {}

AudioTrack::AudioTrack(sdp::Media description) 
    : MediaTrack(std::move(description)) {}

AudioTrack::~AudioTrack() {}
    
} // namespace naivertc
