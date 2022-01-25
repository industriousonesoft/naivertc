#ifndef _RTC_RTP_RTCP_RTP_PACKET_SEQUENCER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SEQUENCER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_structs.hpp"

namespace naivertc {

// Interface for a class that can assign RTP sequence numbers for a packet
// to be sent.
class SequenceNumberAssigner {
public:
    SequenceNumberAssigner() = default;
    virtual ~SequenceNumberAssigner() = default;

    virtual bool Sequence(RtpPacketToSend& packet) = 0;
};

// NOTE: This class is not thread safe, the caller must provide that.
class RTC_CPP_EXPORT RtpPacketSequencer : public SequenceNumberAssigner {
public:
    // If |require_marker_before_media_padding| is true, padding packets on the media ssrc
    // is not allowed unless the last sequenced media packet had the marker bit set (i.e. don't
    // insert padding packets between the first and last packts of a video frame)
    RtpPacketSequencer(const RtpConfiguration& config);
    ~RtpPacketSequencer();

    uint16_t media_seq_num() const { return media_seq_num_; }
    void set_media_seq_num(uint16_t sequence_num) { media_seq_num_ = sequence_num; }

    uint16_t rtx_seq_num() const { return rtx_seq_num_; }
    void set_rtx_seq_num(uint16_t sequence_num) { rtx_seq_num_ = sequence_num; }

    // Assigns sequence number, and in the case of non-RTX padding also timestamps and payload type.
    // Returns false if sequencing failed, which it can do for instance if the packet to sequence
    // is padding on the media ssrc, but the media is mid frame (the last marker bit is false).
    bool Sequence(RtpPacketToSend& packet) override;

    void SetRtpState(const RtpState& state);
    void PupulateRtpState(RtpState& state);

private:
    void UpdateLastPacketState(const RtpPacketToSend& packet);
    bool PopulatePaddingFields(RtpPacketToSend& packet);

private:
    const uint32_t media_ssrc_;
    const std::optional<uint32_t> rtx_ssrc_;
    const bool require_marker_before_media_padding_;
    std::shared_ptr<Clock> clock_;

    uint16_t media_seq_num_;
    uint16_t rtx_seq_num_;

    int8_t last_payload_type_;
    uint32_t last_rtp_timestamp_;
    int64_t last_capture_time_ms_;
    int64_t last_timestamp_time_ms_;
    bool last_packet_marker_bit_;
};
    
} // namespace naivertc


#endif