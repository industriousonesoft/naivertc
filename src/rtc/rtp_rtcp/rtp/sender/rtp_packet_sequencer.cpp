#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"

namespace naivertc {

namespace {

// Timestamps use a 90kHz clock.
constexpr uint32_t kTimestampTicksPerMs = 90;

constexpr int8_t kInvalidPayloadType = -1;

}  // namespace


RtpPacketSequencer::RtpPacketSequencer(uint32_t media_ssrc, 
                                       uint32_t rtx_ssrc, 
                                       bool require_marker_before_media_padding, 
                                       std::shared_ptr<Clock> clock) 
    : media_ssrc_(media_ssrc),
      rtx_ssrc_(rtx_ssrc),
      require_marker_before_media_padding_(require_marker_before_media_padding),
      clock_(clock),
      media_sequence_num_(0),
      rtx_sequence_num_(0),
      last_payload_type_(kInvalidPayloadType),
      last_rtp_timestamp_(0),
      last_capture_time_ms_(0),
      last_timestamp_time_ms_(0),
      last_packet_marker_bit_(false) {

}

RtpPacketSequencer::~RtpPacketSequencer() {}

bool RtpPacketSequencer::AssignSequenceNumber(std::shared_ptr<RtpPacketToSend> packet) {
    if (packet->packet_type() == RtpPacketType::PADDING && !PopulatePaddingFields(packet)) {
        // This padding packet can't be sent with current state, return without
        // updating the sequence number.
        return false;
    }

    if (packet->ssrc() == media_ssrc_) {
        packet->set_sequence_number(media_sequence_num_++);
        if (packet->packet_type() != RtpPacketType::PADDING) {
            UpdateLastPacketState(packet);
        }
        return true;
    }

    if(packet->ssrc() == rtx_ssrc_) {
        packet->set_sequence_number(rtx_sequence_num_++);
        return true;
    }
    
    return false;
}

void RtpPacketSequencer::SetRtpState(const RtpState& state) {
    media_sequence_num_ = state.sequence_num;
    last_rtp_timestamp_ = state.timestamp;
    last_capture_time_ms_ = state.capture_time_ms;
    last_timestamp_time_ms_ = state.last_timestamp_time_ms;
}

void RtpPacketSequencer::PupulateRtpState(RtpState& state) {
    state.sequence_num = media_sequence_num_;
    state.timestamp = last_rtp_timestamp_;
    state.capture_time_ms = last_capture_time_ms_;
    state.last_timestamp_time_ms = last_timestamp_time_ms_;
}

// Private methods
void RtpPacketSequencer::UpdateLastPacketState(std::shared_ptr<const RtpPacketToSend> packet) {
    // Remember marker bit to determine if padding can be inserted with
    // sequence number fallow |packet|
    last_packet_marker_bit_ = packet->marker();

    // Remember media payload type to use in the padding packet if rtx 
    // is disabled.
    // FIXME: 是否将is_red判断删除？原因如下：
    // 由于在RtpVideoSender中并没有新建RED包，而是通过is_red标识，
    // 因此payload_type就是meida的payload_type而非RED的payload_type
    if (packet->is_red()) {
        // last_payload_type_ = packet->payload().data()[0];
        last_payload_type_ = packet->payload_type();
    }else {
        last_payload_type_ = packet->payload_type();
    }
    // Save timestamps to generate timestamp field and extensions for the padding
    last_rtp_timestamp_ = packet->timestamp();
    last_timestamp_time_ms_ = clock_->TimeInMs();
    last_capture_time_ms_ = packet->capture_time_ms();

}

bool RtpPacketSequencer::PopulatePaddingFields(std::shared_ptr<RtpPacketToSend> packet) {
    if (packet->ssrc() == media_ssrc_) {
        if (last_payload_type_ == kInvalidPayloadType) {
            return false;
        }

        // Without RTX we can't send padding in the middle of frames.
        // For audio marker bits dosen't mark the end of a frame and
        // frames are usually a single packet, so for now we don't apply
        // this rule for audio.
        if (require_marker_before_media_padding_ && !last_packet_marker_bit_) {
            return false;
        }

        packet->set_timestamp(last_rtp_timestamp_);
        packet->set_capture_time_ms(last_capture_time_ms_);
        packet->set_payload_type(last_payload_type_);
        return true;
    }else if (packet->ssrc() == rtx_ssrc_) {
        if (packet->payload_size() > 0) {
            // This is payload padding packet, don't update timestamp fields/
            return true;
        }

        packet->set_timestamp(last_rtp_timestamp_);
        packet->set_capture_time_ms(last_capture_time_ms_);

        // Only change the timestamp of padding packets sent over RTX.
        // Padding only packets over RTP has to be sent as part of a 
        // media frame (and therefore the same timestamp).
        int64_t now_ms = clock_->TimeInMs();
        if (last_timestamp_time_ms_ > 0) {
            packet->set_timestamp(packet->timestamp() + (now_ms - last_timestamp_time_ms_) * kTimestampTicksPerMs);
            if (packet->capture_time_ms() > 0) {
                packet->set_capture_time_ms(packet->capture_time_ms() + now_ms - last_timestamp_time_ms_);
            }
        }
        return true;
    }
    return false;
}
    
} // namespace naivertc
