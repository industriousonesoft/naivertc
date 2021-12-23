#ifndef _RTC_CHANNELS_MEDIA_CHANNEL_H_
#define _RTC_CHANNELS_MEDIA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/channels/channel.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/api/media_transport.hpp"

#include <iostream>

namespace naivertc {

// MediaChannel
class RTC_CPP_EXPORT MediaChannel : public Channel {
public:
    enum class Kind {
        UNKNOWN,
        VIDEO,
        AUDIO
    };
public:
    MediaChannel(Kind kind, std::string mid);
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
    TaskQueue task_queue_;
    bool is_opened_ = false;

    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;

    MediaTransport* transport_;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaChannel::Kind kind);

} // nemespace naivertc

#endif