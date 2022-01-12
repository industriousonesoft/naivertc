#include "rtc/call/rtp_receive_statistics.hpp"
#include "testing/simulated_clock.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {
    
constexpr size_t kPacketSize1 = 100;
constexpr size_t kPacketSize2 = 300;
constexpr uint32_t kSsrc1 = 101;
constexpr uint32_t kSsrc2 = 202;
constexpr uint32_t kSsrc3 = 203;
constexpr uint32_t kSsrc4 = 304;

RtpPacketReceived CreateRtpPacket(uint32_t ssrc, 
                                  size_t header_size, 
                                  size_t payload_size, 
                                  size_t padding_size) {
    RtpPacketReceived packet_received;
    packet_received.set_ssrc(ssrc);
    packet_received.set_sequence_number(100);
    packet_received.set_payload_type_frequency(90000);
    EXPECT_GE(header_size, 12);
    EXPECT_EQ(header_size % 4, 0);

    if (header_size > 12) {
        const int num_csrcs = (header_size - 12) / 4;
        std::vector<uint32_t> csrcs(num_csrcs);
        packet_received.set_csrcs(csrcs);
    }
    packet_received.SetPayloadSize(payload_size);
    packet_received.SetPadding(padding_size);
    return packet_received;
}

RtpPacketReceived CreateRtpPacket(uint32_t ssrc, size_t packet_size) {
    return CreateRtpPacket(ssrc, 12, packet_size - 12, 0);
}

void IncrementSeqNum(RtpPacketReceived* packet, uint16_t incr) {
    packet->set_sequence_number(packet->sequence_number() + incr);
}

void IncrementSeqNum(RtpPacketReceived* packet) {
    IncrementSeqNum(packet, 1);
}

} // namespace

class T(RtpReceiveStatisticsTest) {
public:
    T(RtpReceiveStatisticsTest)() 
        : clock_(0),
          receive_statistics_(&clock_) {
        packet1_ = CreateRtpPacket(kSsrc1, kPacketSize1);
        packet2_ = CreateRtpPacket(kSsrc2, kPacketSize2);
    }

protected:
    SimulatedClock clock_;
    RtpReceiveStatistics receive_statistics_;
    RtpPacketReceived packet1_;
    RtpPacketReceived packet2_;
};

    
} // namespace test
} // namespace naivertc
