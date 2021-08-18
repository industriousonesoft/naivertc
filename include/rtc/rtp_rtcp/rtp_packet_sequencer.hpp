#ifndef _RTC_RTP_RTCP_RTP_PACKET_SEQUENCER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SEQUENCER_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/rtp_rtcp/rtp_packet_to_send.hpp"

namespace naivertc {

// This class is not thread safe, the caller must provide that.
class RTC_CPP_EXPORT RtpPacketSequencer {
public:
    // If |require_marker_before_media_padding| is true, padding packets on the media ssrc
    // is not allowed unless the last sequenced media packet had the marker bit set (i.e. don't
    // insert padding packets between the first and last packts of a video frame)
    RtpPacketSequencer(uint32_t media_ssrc, uint32_t rtx_ssrc, bool require_marker_before_media_padding, Clock* clock);
    ~RtpPacketSequencer();

    uint16_t media_sequence_num() const { return media_sequence_num_; }
    void set_media_sequence_num(uint16_t sequence_num) { media_sequence_num_ = sequence_num; }

    uint16_t rtx_sequence_num() const { return rtx_sequence_num_; }
    void set_rtx_sequence_num(uint16_t sequence_num) { rtx_sequence_num_ = sequence_num; }

    // Assigns sequence number, and in the case of non-RTX padding also timestamps and payload type.
    // Returns false if sequencing failed, which it can do for instance if the packet to sequence
    // is padding on the media ssrc, but the media is mid frame (the last marker bit is false).
    bool Sequence(RtpPacketToSend& packet);

    void SetRtpState(const RtpState& state);
    void PupulateRtpState(RtpState& state);

private:
    void UpdateLastPacketState(const RtpPacketToSend& packet);
    bool PopulatePaddingFields(RtpPacketToSend& packet);

private:
    const uint32_t media_ssrc_;
    const uint32_t rtx_ssrc_;
    const bool require_marker_before_media_padding_;
    Clock* const clock_;

    uint16_t media_sequence_num_;
    uint16_t rtx_sequence_num_;

    int8_t last_payload_type_;
    uint32_t last_rtp_timestamp_;
    int64_t last_capture_time_ms_;
    int64_t last_timestamp_time_ms_;
    bool last_packet_marker_bit_;
};
    
} // namespace naivertc


#endif