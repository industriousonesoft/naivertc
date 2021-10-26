#include "rtc/call/rtp_video_stream_receiver.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

// TODO: Change kPacketBufferStartSize back to 32 in M63 see: crbug.com/752886
constexpr int kPacketBufferStartSize = 512;
constexpr int kPacketBufferMaxSize = 2048;

constexpr int kPacketLogIntervalMs = 10000;

std::shared_ptr<RtcpModule> CreateRtcpModule(const RtpVideoStreamReceiver::Configuration& stream_config,
                                             std::shared_ptr<Clock> clock,
                                             std::shared_ptr<TaskQueue> task_queue) {
    RtcpConfiguration rtcp_config;
    rtcp_config.audio = false;
    rtcp_config.local_media_ssrc = stream_config.local_ssrc;
    rtcp_config.remote_ssrc = stream_config.remote_ssrc;
    return std::make_shared<RtcpModule>(rtcp_config, task_queue);
}

std::unique_ptr<rtc::video::FrameToDecode> CreateFrameToDecode(const rtc::video::jitter::PacketBuffer::Frame& assembled_frame, 
                                                               int64_t estimated_ntp_time_ms) {
    return std::make_unique<rtc::video::FrameToDecode>(assembled_frame.frame_type,
                                                        assembled_frame.codec_type,
                                                        assembled_frame.seq_num_start,
                                                        assembled_frame.seq_num_end,
                                                        assembled_frame.timestamp,
                                                        estimated_ntp_time_ms,
                                                        assembled_frame.times_nacked,
                                                        assembled_frame.min_received_time_ms,
                                                        assembled_frame.max_received_time_ms,
                                                        std::move(assembled_frame.bitstream));
}

} // namespace

// RtpVideoStreamReceiver
RtpVideoStreamReceiver::RtpVideoStreamReceiver(Configuration config,
                                               std::shared_ptr<Clock> clock,
                                               std::shared_ptr<TaskQueue> task_queue) 
    : config_(std::move(config)),
      clock_(clock),
      task_queue_(std::move(task_queue)),
      rtcp_module_(CreateRtcpModule(config_, clock_, task_queue)),
      rtcp_feedback_buffer_(std::make_shared<RtcpFeedbackBuffer>(rtcp_module_, rtcp_module_)),
      nack_module_(config.nack_enabled ? std::make_unique<NackModule>(clock_, 
                                                                      task_queue_, 
                                                                      rtcp_feedback_buffer_, 
                                                                      rtcp_feedback_buffer_)
                                       : nullptr),
      packet_buffer_(kPacketBufferStartSize, kPacketBufferMaxSize),
      remote_ntp_time_estimator_(clock_),
      last_packet_log_ms_(-1) {}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {}

void RtpVideoStreamReceiver::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    task_queue_->Async([this, rtcp_packet=std::move(in_packet)](){
        rtcp_module_->IncomingPacket(std::move(rtcp_packet));

        int64_t last_rtt_ms = 0;
        rtcp_module_->RTT(config_.remote_ssrc, 
                          &last_rtt_ms, 
                          nullptr /* avg_rtt_ms */, 
                          nullptr /* min_rtt_ms */, 
                          nullptr /* max_rtt_ms */);
        // Waiting for valid rtt.
        if (last_rtt_ms == 0) {
            return true;
        }

        uint32_t received_ntp_secs = 0;
        uint32_t received_ntp_frac = 0;
        uint32_t rtcp_arrival_time_secs = 0;
        uint32_t rtcp_arrival_time_frac = 0;
        uint32_t rtp_timestamp = 0;
        if (rtcp_module_->RemoteNTP(&received_ntp_secs, 
                                    &received_ntp_frac, 
                                    &rtcp_arrival_time_secs,
                                    &rtcp_arrival_time_frac, 
                                    &rtp_timestamp) != 0) {
            // Waiting for RTCP.
            return true;
        }
        NtpTime rtcp_arrival_ntp(rtcp_arrival_time_secs, rtcp_arrival_time_frac);
        int64_t time_since_rtcp_arrival = clock_->now_ntp_time_ms() - rtcp_arrival_ntp.ToMs();
        // Don't use old SRs to estimate time.
        if (time_since_rtcp_arrival <= 1 /* 1 ms */) {
            remote_ntp_time_estimator_.UpdateRtcpTimestamp(last_rtt_ms, received_ntp_secs, received_ntp_frac, rtp_timestamp);
            std::optional<int64_t> remote_to_local_clock_offset_ms = remote_ntp_time_estimator_.EstimateRemoteToLocalClockOffsetMs();
            if (remote_to_local_clock_offset_ms) {
                PLOG_INFO << "Estimated offset in ms: " << remote_to_local_clock_offset_ms.value()
                          << " between remote and local clock.";
                // TODO: Update capture_clock_offset_updater_??
            }
        }   
        return true;
    });
}

void RtpVideoStreamReceiver::OnRtpPacket(RtpPacketReceived in_packet) {
    task_queue_->Async([this, in_packet=std::move(in_packet)](){
        this->OnReceivedPacket(std::move(in_packet));
    });
}

// TODO: Using RtpPacketReceived as parameters for thread-safety
void RtpVideoStreamReceiver::OnRecoveredPacket(const uint8_t* packet, size_t packet_size) {
    task_queue_->Async([this, packet, packet_size](){
        RtpPacketReceived recovered_packet;
        if (!recovered_packet.Parse(packet, packet_size)) {
            PLOG_WARNING << "Failed to parse recovered packet as RTP packet.";
            return;
        }
        if (recovered_packet.payload_type() == config_.red_payload_type) {
            PLOG_WARNING << "Discarding recovered packet with RED encapsulation.";
            return;
        }

        // TODO: Identify header extensions of RTP packet.
        recovered_packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);

        this->OnReceivedPacket(std::move(recovered_packet));
    });
}

// Private methods
void RtpVideoStreamReceiver::OnReceivedPacket(const RtpPacketReceived& packet) {
    // Padding or keep-alive packet
    if (packet.payload_size() == 0) {
        HandleEmptyPacket(packet.sequence_number());
        return;
    }
    if (packet.payload_type() == config_.red_payload_type) {
        HandleRedPacket(packet);
        return;
    }
    const auto type_it = payload_type_map_.find(packet.payload_type());
    if (type_it == payload_type_map_.end()) {
        PLOG_WARNING << "No RTP depacketizer found for payload type=" 
                     << packet.payload_type();
        return;
    }
    std::optional<RtpDepacketizer::Packet> depacketized_packet = type_it->second->Depacketize(packet.PayloadBuffer());
    if (!depacketized_packet) {
        PLOG_WARNING << "Failed to depacketize RTP payload.";
        return;
    }

    OnDepacketizedPacket(std::move(*depacketized_packet), packet);
}

void RtpVideoStreamReceiver::OnDepacketizedPacket(RtpDepacketizer::Packet depacketized_packet, 
                                                   const RtpPacketReceived& rtp_packet) {
    auto packet = std::make_unique<rtc::video::jitter::PacketBuffer::Packet>(depacketized_packet.video_header,
                                                                              depacketized_packet.video_codec_header,
                                                                              rtp_packet.sequence_number(),
                                                                              rtp_packet.timestamp());
    RtpVideoHeader& video_header = packet->video_header;
    video_header.is_last_packet_in_frame |= rtp_packet.marker();

    // TODO: Collect packet info

    if (auto extension = rtp_packet.GetExtension<rtp::PlayoutDelayLimits>()) {
        video_header.playout_delay.min_ms = extension->min_ms();
        video_header.playout_delay.max_ms = extension->max_ms();
    }

    // TODO: Support more RTP header extensions

    if (!rtp_packet.is_recovered()) {
        UpdatePacketReceiveTimestamps(rtp_packet, video_header.frame_type == VideoFrameType::KEY);
    }

    if (nack_module_) {
        // Using first packet of the keyframe to indicate the keyframe is coming.
        const bool is_keyframe = video_header.is_first_packet_in_frame && 
                                 video_header.frame_type == VideoFrameType::KEY;
        // Return the nacks has sent for the packet.
        packet->times_nacked = nack_module_->InsertPacket(rtp_packet.sequence_number(), is_keyframe, rtp_packet.is_recovered());
    }else {
        // Indicates the NACK mechanism is disable.
        packet->times_nacked = -1;
    }

    if (depacketized_packet.video_payload.empty()) {
        HandleEmptyPacket(rtp_packet.sequence_number());
        rtcp_feedback_buffer_->SendBufferedRtcpFeedbacks();
        return;
    }

    // H264
    if (video_header.codec_type == VideoCodecType::H264) {
        auto h264_header = std::get<h264::PacketizationInfo>(packet->video_codec_header);
        h264::SpsPpsTracker::FixedBitstream fixed = h264_sps_pps_tracker_.CopyAndFixBitstream(video_header.is_first_packet_in_frame, 
                                                                                              video_header.frame_width, 
                                                                                              video_header.frame_height, 
                                                                                              h264_header,
                                                                                              depacketized_packet.video_payload);
        switch (fixed.action) {
        case h264::SpsPpsTracker::PacketAction::REQUEST_KEY_FRAME:
            rtcp_feedback_buffer_->RequestKeyFrame();
            rtcp_feedback_buffer_->SendBufferedRtcpFeedbacks();
            PLOG_WARNING << "IDR as the first packet in frame without SPS and PPS, droping.";
            return;
        case h264::SpsPpsTracker::PacketAction::DROP:
            PLOG_WARNING << "Packet truncated, droping.";
            return;
        case h264::SpsPpsTracker::PacketAction::INSERT:
            packet->video_payload = std::move(depacketized_packet.video_payload);
            break;
        }
    } else {
        packet->video_payload = std::move(depacketized_packet.video_payload);
    }

    rtcp_feedback_buffer_->SendBufferedRtcpFeedbacks();
    OnInsertedPacket(packet_buffer_.InsertPacket(std::move(packet)));
}

void RtpVideoStreamReceiver::OnInsertedPacket(rtc::video::jitter::PacketBuffer::InsertResult result) {
    std::vector<ArrayView<const uint8_t>> packet_payloads;
    for (const auto& frame : result.assembled_frames) {
        auto frame_to_decode = CreateFrameToDecode(*(frame.get()), remote_ntp_time_estimator_.Estimate(frame->timestamp));
        OnAssembledFrame(std::move(frame_to_decode));
    }
    if (result.keyframe_requested) {
        last_received_system_time_.reset();
        last_received_keyframe_system_time_.reset();
        last_received_keyframe_timestamp_.reset();
        RequestKeyFrame();
    }
}

void RtpVideoStreamReceiver::OnAssembledFrame(std::unique_ptr<rtc::video::FrameToDecode> frame) {
    if (!has_received_frame_) {
        // If frames arrive before a key frame, they would not be decodable.
        // In that case, request a key frame ASAP.
        if (frame->frame_type() == VideoFrameType::KEY) {
            RequestKeyFrame();
        }
        has_received_frame_ = true;
    }

    // Switch `frame_ref_finder_` if necessary.
    SwitchFrameRefFinderIfNecessary(*(frame.get()));

    frame_ref_finder_->InsertFrame(std::move(frame));
}

void RtpVideoStreamReceiver::OnCompleteFrame(std::unique_ptr<rtc::video::FrameToDecode> frame) {
    last_seq_num_for_pic_id_[frame->id()] = frame->seq_num_end();
    last_completed_picture_id_ = std::max(last_completed_picture_id_, frame->id());
}

void RtpVideoStreamReceiver::RequestKeyFrame() {
    // TODO: Send PictureLossIndication by rtcp_module
}

void RtpVideoStreamReceiver::HandleEmptyPacket(uint16_t seq_num) {
    if (frame_ref_finder_) {
        frame_ref_finder_->InsertPadding(seq_num);
    }
    OnInsertedPacket(packet_buffer_.InsertPadding(seq_num));
    if (nack_module_) {
        nack_module_->InsertPacket(seq_num, false /* is_keyframe */, false /* is_recovered */);
    }
}

void RtpVideoStreamReceiver::HandleRedPacket(const RtpPacketReceived& packet) {
    if (packet.payload_type() == config_.red_payload_type && 
        packet.payload_size() > 0) {
        if (packet.payload()[0] == config_.ulpfec_payload_type) {
            // Handle packet recovered by FEC as a empty packet to 
            // avoid NACKing it.
            HandleEmptyPacket(packet.sequence_number());
        }

        // TODO: Handle RED packet using UlpFEC receiver.
    }
}

void RtpVideoStreamReceiver::UpdatePacketReceiveTimestamps(const RtpPacketReceived& packet, bool is_keyframe) {
    Timestamp now = clock_->CurrentTime();
    if (is_keyframe || last_received_keyframe_timestamp_ == packet.timestamp()) {
        last_received_keyframe_timestamp_ = packet.timestamp();
        last_received_keyframe_system_time_ = now;
    }
    last_received_system_time_ = now;
    last_received_keyframe_timestamp_ = packet.timestamp();
    if (now.ms() - last_packet_log_ms_ > kPacketLogIntervalMs) {
        PLOG_INFO << "Packet received on SSRC: " << packet.ssrc()
                  << " with payload type: " << static_cast<int>(packet.payload_type())
                  << ", timestamp: " << packet.timestamp()
                  << ", sequence number: " << packet.sequence_number()
                  << ", arrival time ms: " << packet.arrival_time().ms();
        last_packet_log_ms_ = now.ms();
    }
}

void RtpVideoStreamReceiver::SwitchFrameRefFinderIfNecessary(const rtc::video::FrameToDecode& frame) {
    if (curr_codec_type_) {
        bool frame_is_newer = wrap_around_utils::AheadOf<uint32_t>(frame.timestamp(), last_assembled_frame_rtp_timestamp_);
        if (frame.codec_type() != curr_codec_type_) {
            if (frame_is_newer) {
                // When we reset the `frame_ref_finder_` we don't want new picture ids
                // to overlap with old picture ids. To ensure that doesn't happen we
                // start from the `last_completed_picture_id_` and add an offset in case
                // of reordering.
                curr_codec_type_ = frame.codec_type();
                // FIXME: Why we use uint16_max as the gap value?
                int64_t picture_id_offset = last_completed_picture_id_ + std::numeric_limits<uint16_t>::max();
                CreateFrameRefFinder(curr_codec_type_.value(), picture_id_offset);
            }else {
                // Old frame from before the codec switch, discard it.
                return;
            }
        }
        if (frame_is_newer) {
            last_assembled_frame_rtp_timestamp_ = frame.timestamp();
        }
    }else {
        curr_codec_type_ = frame.codec_type();
        last_assembled_frame_rtp_timestamp_ = frame.timestamp();
        CreateFrameRefFinder(curr_codec_type_.value(), 0 /* picture_id_offset */);
    }
}

void RtpVideoStreamReceiver::CreateFrameRefFinder(VideoCodecType codec_type, int64_t picture_id_offset) {
    frame_ref_finder_.reset();
    frame_ref_finder_ = rtc::video::jitter::FrameRefFinder::Create(codec_type, picture_id_offset);
    frame_ref_finder_->OnFrameRefFound(std::bind(&RtpVideoStreamReceiver::OnCompleteFrame, this, std::placeholders::_1));
}

} // namespace naivertc
