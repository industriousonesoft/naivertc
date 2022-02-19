#ifndef _RTC_CHANNELS_MEDIA_CHANNEL_H_
#define _RTC_CHANNELS_MEDIA_CHANNEL_H_

#include "base/defines.hpp"

namespace naivertc {

// MediaChannel
class MediaChannel {
public:
    virtual ~MediaChannel() = default;
    virtual bool is_opened() const = 0;
    virtual void Open() = 0;
    virtual void Close() = 0;
};

} // nemespace naivertc

#endif