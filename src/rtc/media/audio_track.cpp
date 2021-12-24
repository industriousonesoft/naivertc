#include "rtc/media/audio_track.hpp"

namespace naivertc {

AudioTrack::AudioTrack(const Configuration& config, TaskQueue* task_queue) 
    : MediaTrack(config, task_queue) {}

AudioTrack::AudioTrack(sdp::Media remote_description, TaskQueue* task_queue) 
    : MediaTrack(std::move(remote_description), task_queue) {}

AudioTrack::~AudioTrack() {}
    
} // namespace naivertc
