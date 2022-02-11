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
    // Kind
    enum class Kind {
        VIDEO,
        AUDIO
    };

    using OpenedCallback = std::function<void()>;
    using ClosedCallback = std::function<void()>;

public:
    virtual ~MediaChannel();

    Kind kind() const;
    const std::string mid() const;

    bool is_opened() const;
    
    virtual void Open();
    virtual void Close();

    void OnOpened(OpenedCallback callback);
    void OnClosed(ClosedCallback callback);

protected:
    MediaChannel(Kind kind, 
                 std::string mid);

private:
    void TriggerOpen();
    void TriggerClose();

    void CreateStreams();

protected:
    const Kind kind_;
    const std::string mid_;
    TaskQueueImpl* const signaling_queue_;
    
    bool is_opened_ = false;
    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;
};

} // nemespace naivertc

#endif