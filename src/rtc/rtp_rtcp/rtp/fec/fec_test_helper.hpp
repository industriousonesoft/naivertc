#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_TEST_HELPER_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_TEST_HELPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "common/utils_random.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

namespace naivertc {
namespace test {

// RtpPacketGenerator
class RtpPacketGenerator {
public:
    explicit RtpPacketGenerator(uint32_t ssrc, uint8_t payload_type);
    virtual ~RtpPacketGenerator();

    void NewFrame(size_t num_packets);

    RtpPacket NextRtpPacket(size_t payload_size, size_t padding_size = 0);

protected:
    size_t num_packets_left_;

private:
    uint32_t ssrc_;
    uint8_t payload_type_;
    uint16_t seq_num_;
    uint32_t timestamp_;
};

// UlpFecPacketGenerator
class UlpFecPacketGenerator : public RtpPacketGenerator {
public:
    explicit UlpFecPacketGenerator(uint32_t ssrc, 
                                   uint8_t media_payload_type, 
                                   uint8_t fec_payload_type,
                                   uint8_t red_payload_type);
    ~UlpFecPacketGenerator() override;

    // Encapsulate the media packet as a RED packet.
    RtpPacketReceived BuildMediaRedPacket(const RtpPacket& rtp_packet, bool is_recovered = false);

    // Encapsulate the FEC packet as a RED packet.
    RtpPacketReceived BuildUlpFecRedPacket(const CopyOnWriteBuffer& fec_packets);

private:
    uint8_t fec_payload_type_;
    uint8_t red_payload_type_;
};

} // namespace test
} // namespace naivertc

#endif