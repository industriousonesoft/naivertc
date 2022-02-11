#ifndef _RTC_CHANNELS_MEDIA_CHANNEL_H_
#define _RTC_CHANNELS_MEDIA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/api/rtp_packet_sink.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"

#include <iostream>

namespace naivertc {

class Broadcaster;

// MediaChannel
class RTC_CPP_EXPORT MediaChannel {
public:
    virtual ~MediaChannel() = default;
    virtual bool is_opened() const = 0;
    virtual void Open() = 0;
    virtual void Close() = 0;
};

} // nemespace naivertc

#endif