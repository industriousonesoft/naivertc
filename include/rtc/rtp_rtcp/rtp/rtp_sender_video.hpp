#ifndef _RTC_RTP_RTCP_RTP_VIDEO_SENDER_H_
#define _RTC_RTP_RTCP_RTP_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_video_header.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

#include <memory>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT RtpSenderVideo {
public:
    struct Configuration {
        Configuration() = default;
        Configuration(const Configuration&) = delete;
        Configuration(Configuration&&) = default;

        std::shared_ptr<Clock> clock = nullptr;
        // Codec
        video::CodecType codec_type = video::CodecType::NONE;
      
        std::shared_ptr<RtpSender> packet_sender;
    };
public:
    RtpSenderVideo(const Configuration& config, 
                   std::shared_ptr<TaskQueue> task_queue);
    virtual ~RtpSenderVideo();

    bool SendVideo(int payload_type,
                   uint32_t rtp_timestamp, 
                   int64_t capture_time_ms, 
                   ArrayView<const uint8_t> payload,
                   RtpVideoHeader video_header,
                   std::optional<int64_t> expected_retransmission_time_ms,
                   std::optional<int64_t> estimated_capture_clock_offset_ms = 0);

private:
    void MaybeUpdateCurrentPlayoutDelay(const RtpVideoHeader& header);

    void AddRtpHeaderExtensions(std::shared_ptr<RtpPacketToSend> packet);
private:
    std::shared_ptr<Clock> clock_;
    const video::CodecType codec_type_;
    std::shared_ptr<RtpSender> packet_sender_;
    std::shared_ptr<TaskQueue> task_queue_;

    video::PlayoutDelay current_playout_delay_;

    bool playout_delay_pending_;

    std::shared_ptr<RtpPacketizer> rtp_packetizer_;
    
};
    
} // namespace naivertc


#endif