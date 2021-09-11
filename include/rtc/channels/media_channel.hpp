#ifndef _RTC_CHANNELS_MEDIA_CHANNEL_H_
#define _RTC_CHANNELS_MEDIA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/channels/channel.hpp"
#include "common/task_queue.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"

namespace naivertc {

class RTC_CPP_EXPORT MediaChannel : public Channel {
public:
    MediaChannel(const std::string mid);
    virtual ~MediaChannel();

    const std::string mid() const;

    bool is_opened() const;

    void Open(std::weak_ptr<DtlsSrtpTransport> srtp_transport);
    void Close() override;

    void OnOpened(OpenedCallback callback) override;
    void OnClosed(ClosedCallback callback) override;

private:
    void TriggerOpen();
    void TriggerClose();

protected:
    const std::string mid_;
    TaskQueue task_queue_;
    bool is_opened_ = false;

    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;

    std::weak_ptr<DtlsSrtpTransport> srtp_transport_;
};

}

#endif