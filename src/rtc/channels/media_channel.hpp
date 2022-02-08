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

// NOTE: Using forward declaration instead of including files 
// to speed up compilation.
class MediaSendStream;

// MediaChannel
class RTC_CPP_EXPORT MediaChannel : public RtpPacketSink,
                                    public RtcpPacketSink {
public:
    enum class Kind {
        VIDEO,
        AUDIO
    };

    using OpenedCallback = std::function<void()>;
    using ClosedCallback = std::function<void()>;
public:
    virtual ~MediaChannel() override;

    Kind kind() const;
    const std::string mid() const;

    bool is_opened() const;
    
    virtual void Open(std::weak_ptr<MediaTransport> transport);
    virtual void Close();

    std::vector<uint32_t> send_ssrcs() const;

    void OnOpened(OpenedCallback callback);
    void OnClosed(ClosedCallback callback);

    void OnMediaNegotiated(const sdp::Media local_media, 
                           const sdp::Media remote_media, 
                           sdp::Type remote_sdp_type);

    // Implements RtcpPacketSink
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;
    // Implements RtpPacketSink
    void OnRtpPacket(RtpPacketReceived in_packet) override;

protected:
    MediaChannel(Kind kind, std::string mid, TaskQueue* worker_queue);

private:
    void TriggerOpen();
    void TriggerClose();

    void CreateStreams();

protected:
    const Kind kind_;
    const std::string mid_;
    std::unique_ptr<RealTimeClock> clock_;
    SequenceChecker signaling_queue_checker_;
    TaskQueueImpl* const signaling_queue_;
    TaskQueue* const worker_queue_;

    std::optional<RtpParameters> send_rtp_parameters_;

    bool is_opened_ = false;
    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;

    std::weak_ptr<MediaTransport> send_transport_;

    std::unique_ptr<MediaSendStream> send_stream_;
};

} // nemespace naivertc

#endif