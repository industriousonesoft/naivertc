#ifndef _RTC_CHANNELS_CHANNEL_H_
#define _RTC_CHANNELS_CHANNEL_H_

#include "base/defines.hpp"

namespace naivertc {

class RTC_CPP_EXPORT Channel {
public:
    using OpenedCallback = std::function<void()>;
    using ClosedCallback = std::function<void()>;
public:
    virtual ~Channel();

    virtual void Close() = 0;

    virtual void OnOpened(OpenedCallback callback) = 0;
    virtual void OnClosed(ClosedCallback callback) = 0;
};
    
} // namespace naivertc


#endif