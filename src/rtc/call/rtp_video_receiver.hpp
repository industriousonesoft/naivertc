#ifndef _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_
#define _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtcp_module.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp"
#include "rtc/rtp_rtcp/components/remote_ntp_time_estimator.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/media/video/codecs/h264/sps_pps_tracker.hpp"

#include <memory>
#include <map>
#include <vector>
#include <optional>

namespace naivertc {

class RtpPacketReceived;
class CopyOnWriteBuffer;

class RTC_CPP_EXPORT RtpVideoReceiver : public RecoveredPacketReceiver,
                                        public std::enable_shared_from_this<RtpVideoReceiver> {
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

    // CompleteFrameReceiver
    class CompleteFrameReceiver {
    public:
        virtual ~CompleteFrameReceiver() = default;
        virtual void OnCompleteFrame(rtp::video::FrameToDecode frame) = 0;
    };

public:
    RtpVideoReceiver(Configuration config,
                           std::shared_ptr<Clock> clock,
                           std::shared_ptr<TaskQueue> task_queue,
                           std::weak_ptr<CompleteFrameReceiver> complete_frame_receiver);
    ~RtpVideoReceiver() override;

    void OnRtcpPacket(CopyOnWriteBuffer in_packet);
    void OnRtpPacket(RtpPacketReceived in_packet);

    void OnContinuousFrame(int64_t frame_id);
    void OnDecodedFrame(int64_t frame_id);

    void UpdateRtt(int64_t max_rtt_ms);

    void RequestKeyFrame();

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
    void OnDepacketizedPacket(RtpDepacketizer::Packet depacketized_packet, 
                              const RtpPacketReceived& rtp_packet);
    void OnInsertedPacket(rtp::video::jitter::PacketBuffer::InsertResult result);
    void OnAssembledFrame(rtp::video::FrameToDecode frame);
    void OnCompleteFrame(rtp::video::FrameToDecode frame);

    void HandleEmptyPacket(uint16_t seq_num);
    void HandleRedPacket(const RtpPacketReceived& packet);
    void UpdatePacketReceiveTimestamps(const RtpPacketReceived& packet, bool is_keyframe);

    void CreateFrameRefFinderIfNecessary(const rtp::video::FrameToDecode& frame);
    void CreateFrameRefFinder(VideoCodecType codec_type, int64_t picture_id_offset);

    // Implements RecoveredPacketReceiver.
    void OnRecoveredPacket(CopyOnWriteBuffer packet) override;
    
private:
    const Configuration config_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::weak_ptr<CompleteFrameReceiver> complete_frame_receiver_;
    std::shared_ptr<RtcpModule> rtcp_module_;
    RtcpFeedbackBuffer rtcp_feedback_buffer_;
    std::unique_ptr<NackModule> nack_module_;

    h264::SpsPpsTracker h264_sps_pps_tracker_;
    rtp::video::jitter::PacketBuffer packet_buffer_;
    std::unique_ptr<rtp::video::jitter::FrameRefFinder> frame_ref_finder_;
    RemoteNtpTimeEstimator remote_ntp_time_estimator_;
    UlpFecReceiver ulp_fec_receiver_;

    std::map<uint8_t, std::unique_ptr<RtpDepacketizer>> payload_type_map_;

    bool has_received_frame_ = false;
    std::optional<VideoCodecType> curr_codec_type_ = std::nullopt;
    uint32_t last_assembled_frame_rtp_timestamp_ = 0;
    int64_t last_completed_picture_id_ = 0;

    std::map<int64_t, uint16_t> last_seq_num_for_pic_id_;

    std::optional<uint32_t> last_received_timestamp_;
    std::optional<uint32_t> last_received_keyframe_timestamp_;
    std::optional<Timestamp> last_received_system_time_;
    std::optional<Timestamp> last_received_keyframe_system_time_;

    int64_t last_packet_log_ms_;
};
    
} // namespace naivertc


#endif