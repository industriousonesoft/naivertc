#ifndef _RTC_RTP_RTCP_RTP_PACKERT_TO_SEND_H_
#define _RTC_RTP_RTCP_RTP_PACKERT_TO_SEND_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketToSend : public RtpPacket {
public:
    RtpPacketToSend(size_t capacity);
    RtpPacketToSend(const RtpPacketToSend& packet);
    RtpPacketToSend(RtpPacketToSend&& packet);
    explicit RtpPacketToSend(std::shared_ptr<ExtensionManager> extension_manager);
    RtpPacketToSend(std::shared_ptr<ExtensionManager> extension_manager, size_t capacity);

    RtpPacketToSend& operator=(const RtpPacketToSend& packet);
    RtpPacketToSend& operator=(RtpPacketToSend&& packet);
    ~RtpPacketToSend();

    uint16_t capture_time_ms() const { return capture_time_ms_; }
    void set_capture_time_ms(int64_t time_ms) { capture_time_ms_ = time_ms; }

    RtpPacketType packet_type() const { return packet_type_; }
    void set_packet_type(RtpPacketType type) { packet_type_ = type; }

    bool allow_retransmission() const { return allow_retransmission_; }
    void set_allow_retransmission(bool allowed) { allow_retransmission_ = allowed; }

    std::optional<uint16_t> retransmitted_sequence_number() const { return retransmitted_sequence_number_; }
    void set_retransmitted_sequence_number(uint16_t sequence_number) { retransmitted_sequence_number_ = sequence_number; }

    bool is_first_packet_of_frame() const { return is_first_packet_of_frame_; }
    void set_is_first_packet_of_frame(bool first_packet) { is_first_packet_of_frame_ = first_packet; }

    bool is_key_frame() const { return is_key_frame_; }
    void set_is_key_frame(bool is_key_frame) { is_key_frame_ = is_key_frame; }

    bool fec_protected_packet() const { return fec_protected_packet_; }
    void set_fec_protected_packet(bool protect) { fec_protected_packet_ = protect; }

    bool red_protected_packet() const { return red_protected_packet_; }
    void set_red_protected_packet(bool protect) { red_protected_packet_ = protect; }

    bool is_red() const { return is_red_; }
    void set_is_red(bool is_red) { is_red_ = is_red; }

private:
    int64_t capture_time_ms_ = 0;   
    RtpPacketType packet_type_;
    bool allow_retransmission_ = false;
    std::optional<uint16_t> retransmitted_sequence_number_;
    bool is_first_packet_of_frame_ = false;
    bool is_key_frame_ = false;
    bool fec_protected_packet_ = false;
    // the red protected packet may not be red packet yet, 
    // but the red packet must be red protected packet.
    bool red_protected_packet_ = false;
    bool is_red_ = false;
};
    
} // namespace naivertc

#endif