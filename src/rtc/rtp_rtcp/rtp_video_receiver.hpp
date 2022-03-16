#ifndef _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_
#define _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtcp_responser.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp"
#include "rtc/rtp_rtcp/components/remote_ntp_time_estimator.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/media/video/codecs/h264/sps_pps_tracker.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/base/rtp_packet_sink.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"

#include <memory>
#include <map>
#include <vector>
#include <optional>

namespace naivertc {

class Clock;
class RtpPacketReceived;
class CopyOnWriteBuffer;
class RtpReceiveStatistics;

class RtpVideoReceiver : public RecoveredPacketReceiver,
                         public RtpPacketSink {
public:
    struct Configuration {
        Clock* clock = nullptr;
        RtcMediaTransport* send_transport = nullptr;

        RtpParameters rtp;
    };

    // CompleteFrameReceiver
    class CompleteFrameReceiver {
    public:
        virtual ~CompleteFrameReceiver() = default;
        virtual void OnCompleteFrame(rtp::video::FrameToDecode frame) = 0;
    };

public:
    RtpVideoReceiver(const Configuration& config,
                     RtpReceiveStatistics* rtp_recv_stats,
                     CompleteFrameReceiver* complete_frame_receiver);
    ~RtpVideoReceiver() override;

    const RtpParameters* rtp_params() const;

    void OnRtcpPacket(CopyOnWriteBuffer in_packet);
    void OnRtpPacket(RtpPacketReceived in_packet) override;

    void OnContinuousFrame(int64_t frame_id);
    void OnDecodedFrame(int64_t frame_id);

    void UpdateRtt(int64_t max_rtt_ms);

    void RequestKeyFrame();

private:
    class RtcpFeedbackBuffer : public NackSender,
                               public KeyFrameRequestSender {
    public:
        RtcpFeedbackBuffer(RtcpResponser* sender, 
                           KeyFrameRequestSender* key_frame_request_sender);

        ~RtcpFeedbackBuffer() override;

        void SendNack(const std::vector<uint16_t>& nack_list,
                      bool buffering_allowed) override;

        void RequestKeyFrame() override;

        void SendBufferedRtcpFeedbacks();

    private:
        RtcpResponser* const sender_;
        KeyFrameRequestSender* key_frame_request_sender_;

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
    void CreateFrameRefFinder(video::CodecType codec_type, int64_t picture_id_offset);

    // Implements RecoveredPacketReceiver.
    void OnRecoveredPacket(CopyOnWriteBuffer packet) override;

    bool IsRedPacket(int payload_type) const;
    
private:
    SequenceChecker sequence_checker_;
    Clock* const clock_;
    const RtpParameters rtp_params_;
    CompleteFrameReceiver* complete_frame_receiver_;
    std::unique_ptr<RtcpResponser> rtcp_responser_;
    RtcpFeedbackBuffer rtcp_feedback_buffer_;
    std::unique_ptr<NackModule> nack_module_;

    h264::SpsPpsTracker h264_sps_pps_tracker_;
    rtp::video::jitter::PacketBuffer packet_buffer_;
    std::unique_ptr<rtp::video::jitter::FrameRefFinder> frame_ref_finder_;
    RemoteNtpTimeEstimator remote_ntp_time_estimator_;
    UlpFecReceiver ulp_fec_receiver_;

    std::map<uint8_t, std::unique_ptr<RtpDepacketizer>> payload_type_map_;

    bool has_received_frame_ = false;
    std::optional<video::CodecType> curr_codec_type_ = std::nullopt;
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