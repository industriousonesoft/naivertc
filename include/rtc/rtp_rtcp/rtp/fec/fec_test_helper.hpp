#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_TEST_HELPER_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_TEST_HELPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "common/utils_random.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"

namespace naivertc {
namespace test {

// RtpPacketGenerator
class RTC_CPP_EXPORT RtpPacketGenerator {
public:
    explicit RtpPacketGenerator(uint32_t ssrc);
    virtual ~RtpPacketGenerator();

    void NewFrame(size_t num_packets);

    RtpPacket NextRtpPacket(size_t payload_size, size_t padding_size = 0);

protected:
    size_t num_packets_left_;

private:
    uint32_t ssrc_;
    uint16_t seq_num_;
    uint32_t timestamp_;
};

// UlpFecPacketGenerator
class RTC_CPP_EXPORT UlpFecPacketGenerator : public RtpPacketGenerator {
public:
    explicit UlpFecPacketGenerator(uint32_t ssrc);
    ~UlpFecPacketGenerator() override;
};

} // namespace test
} // namespace naivertc

#endif