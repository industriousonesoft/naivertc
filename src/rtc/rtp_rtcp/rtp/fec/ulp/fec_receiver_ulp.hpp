#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_RECEIVER_ULP_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_RECEIVER_ULP_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_decoder.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"

namespace naivertc {

class UlpFecReceiver {
public:
    // Packet counter
    struct PacketCounter {
        // Number of received packets.
        size_t num_received_packets = 0;
        size_t num_received_bytes = 0;
        // Number of received FEC packets.
        size_t num_received_fec_packets = 0;
        // Number of recovered media packets using FEC.
        size_t num_recovered_packets = 0;
        // Time in ms of the first packet is received.
        int64_t first_packet_arrival_time_ms = -1;
    };

public:
    UlpFecReceiver(uint32_t ssrc, 
                   Clock* clock, 
                   RecoveredPacketReceiver* recovered_packet_receiver);
    ~UlpFecReceiver();

    bool OnRedPacket(const RtpPacketReceived& rtp_packet, uint8_t ulpfec_payload_type);

    PacketCounter packet_counter() const { return packet_counter_; }

private:
    void OnRecoveredPacket(const FecDecoder::RecoveredMediaPacket& recovered_packet);

private:
    const uint32_t ssrc_;
    Clock* const clock_;
    RecoveredPacketReceiver* recovered_packet_receiver_;

    const std::unique_ptr<FecDecoder> fec_decoder_;

    PacketCounter packet_counter_;
};
    
} // namespace naivertc


#endif