#ifndef _RTC_RTP_RTCP_RTP_VIDEO_SENDER_H_
#define _RTC_RTP_RTCP_RTP_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT RtpVideoSender {
public:
    struct Configuration {
        Configuration() = default;
        Configuration(const Configuration&) = delete;
        Configuration(Configuration&&) = default;

        std::shared_ptr<Clock> clock = nullptr;
    };
public:
    explicit RtpVideoSender(const Configuration& config);
    virtual ~RtpVideoSender();

private:
    std::shared_ptr<Clock> clock_;
};
    
} // namespace naivertc


#endif