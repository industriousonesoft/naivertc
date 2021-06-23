#ifndef _PC_SDP_DEFINES_H_
#define _PC_SDP_DEFINES_H_

#include <string>
#include <unordered_map>

namespace naivertc {
namespace sdp {

enum class Type {
    UNSPEC,
    OFFER,
    ANSWER,
    PRANSWER, // provisional answer
    ROLLBACK
};

enum class Role {
    ACT_PASS,
    PASSIVE,
    ACTIVE
};

enum class Direction {
    SEND_ONLY,
    RECV_ONLY,
    SEND_RECV,
    INACTIVE,
    UNKNOWN
};

sdp::Type StringToType(const std::string& type_string);
std::string TypeToString(sdp::Type type);

// Default Opus profile
const std::string DEFAULT_OPUS_AUDIO_PROFILE =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";

const std::string DEFAULT_H264_VIDEO_PROFILE =
    "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1";

}
}

#endif