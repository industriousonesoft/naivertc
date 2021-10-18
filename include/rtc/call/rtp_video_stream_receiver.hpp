#ifndef _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_
#define _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/rtp_video_frame_assembler.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"

#include <memory>
#include <map>
#include <vector>

namespace naivertc {

class RtpPacketReceived;
class CopyOnWriteBuffer;

class RTC_CPP_EXPORT RtpVideoStreamReceiver {
public:
    struct Configuration {
        // Sender SSRC used for sending RTCP (such as receiver reports).
        uint32_t local_ssrc = 0;
        // Synchronization source to be received.
        uint32_t remote_ssrc = 0;

        int ulpfec_payload_type = -1;
        int red_payload_type = -1;

        // For RTX to be enabled, both rtx_ssrc and maping are needed.
        uint32_t rtx_ssrc = 0;
        // Map from RTX payload type -> media payload type.
        std::map<int, int> rtx_associated_payload_types;

        // Set if the stream is protected using FlexFEC.
        bool protected_by_flexfec = false;

        bool nack_enabled = false;
    };
public:
    RtpVideoStreamReceiver(Configuration config,
                           std::shared_ptr<Clock> clock,
                           std::shared_ptr<TaskQueue> task_queue);
    ~RtpVideoStreamReceiver();

    void OnRtcpPacket(CopyOnWriteBuffer in_packet);
    void OnRtpPacket(RtpPacketReceived in_packet);

    // Packet recovered by FlexFEX or UlpFEC
    void OnRecoveredPacket(const uint8_t* packet, size_t packet_size);

private:
    class RtcpFeedbackBuffer : public NackSender,
                               public KeyFrameRequestSender {
    public:
        RtcpFeedbackBuffer(std::weak_ptr<NackSender> nack_sender, 
                           std::weak_ptr<KeyFrameRequestSender> key_frame_request_sender);

        ~RtcpFeedbackBuffer() override;

        void SendNack(std::vector<uint16_t> nack_list,
                      bool buffering_allowed) override;

        void RequestKeyFrame() override;

        void SendBufferedRtcpFeedbacks();

    private:
        std::weak_ptr<NackSender> nack_sender_;
        std::weak_ptr<KeyFrameRequestSender> key_frame_request_sender_;

        bool request_key_frame_;
        std::vector<uint16_t> buffered_nack_list_;
    };

private:
    void OnReceivedPacket(const RtpPacketReceived& packet);
    void HandleEmptyPacket(uint16_t seq_num);
    void HandleRedPacket(const RtpPacketReceived& packet);

    void OnDepacketizedPayload(RtpDepacketizer::DepacketizedPayload depacketized_payload, 
                               const RtpPacketReceived& packet);
private:
    const Configuration config_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::unique_ptr<NackModule> nack_module_;

    std::map<uint8_t, std::unique_ptr<RtpDepacketizer>> payload_type_map_;
};
    
} // namespace naivertc


#endif