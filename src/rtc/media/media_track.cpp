#include "rtc/media/media_track.hpp"

#include <plog/Log.h>

namespace naivertc {


// Media track
MediaTrack::MediaTrack(const Configuration& config) 
    : MediaTrack(config.kind(), config.mid()) {}

MediaTrack::MediaTrack(Kind kind, std::string mid) 
    : MediaChannel(kind, mid) {}

MediaTrack::~MediaTrack() {}

bool MediaTrack::IsValidConfig(const Configuration& config) {
    return task_queue_.Sync<bool>([this, &config](){
        if (config.kind() != kind_) {
            PLOG_WARNING << "Failed to reconfig as the incomming kind=" << config.kind()
                         << " is different from media track kind=" << kind_;
            return false;
        } else if (config.mid() != mid_) {
            PLOG_WARNING << "Failed to reconfig as the incomming mid=" << config.mid()
                         << " is different from local media mid=" << mid_;
            return false;
        }
        return true;
    });
}

} // namespace naivertc