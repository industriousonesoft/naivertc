#ifndef _RTC_CHANNELS_MEDIA_CHANNEL_H_
#define _RTC_CHANNELS_MEDIA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/channels/channel.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/base/task_utils/task_queue.hpp"

#include <iostream>

namespace naivertc {

// MediaChannel
class RTC_CPP_EXPORT MediaChannel : public Channel {
public:
    enum class Kind {
        VIDEO,
        AUDIO
    };
public:
    MediaChannel(Kind kind, std::string mid, TaskQueue* task_queue);
    virtual ~MediaChannel();

    Kind kind() const;
    const std::string mid() const;

    bool is_opened() const;

    void Open(MediaTransport* transport);
    void Close() override;

    void OnOpened(OpenedCallback callback) override;
    void OnClosed(ClosedCallback callback) override;

private:
    void TriggerOpen();
    void TriggerClose();

protected:
    const Kind kind_;
    const std::string mid_;
    std::unique_ptr<RealTimeClock> clock_;
    TaskQueue* const task_queue_;
    bool is_opened_ = false;

    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;
    
    MediaTransport* send_transport_ = nullptr;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaChannel::Kind kind);

} // nemespace naivertc

#endif