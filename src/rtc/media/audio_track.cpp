#include "rtc/media/audio_track.hpp"

namespace naivertc {

AudioTrack::AudioTrack(const Configuration& config) 
    : MediaTrack(config) {}

AudioTrack::AudioTrack(sdp::Media remote_description) 
    : MediaTrack(std::move(remote_description)) {}

AudioTrack::~AudioTrack() {}
    
} // namespace naivertc
