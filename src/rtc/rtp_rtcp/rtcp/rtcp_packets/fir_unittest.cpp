#include "rtc/rtp_rtcp/rtcp/rtcp_packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace rtcp {

namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;
constexpr uint8_t kSeqNr = 13;
// Manually created Fir packet matching constants above.
constexpr uint8_t kPacket[] = {0x84, 206,  0x00, 0x04, 0x12, 0x34, 0x56,
                               0x78, 0x00, 0x00, 0x00, 0x00, 0x23, 0x45,
                               0x67, 0x89, 0x0d, 0x00, 0x00, 0x00};
constexpr size_t kPacketSize = sizeof(kPacket);
} // namespace

MY_TEST(RtcpFirTest, Parse) {
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, kPacketSize));
    EXPECT_EQ(Fir::kPacketType, common_header.type());
    EXPECT_EQ(Fir::kFeedbackMessageType, common_header.feedback_message_type());
    EXPECT_EQ(kPacketSize - 4, common_header.payload_size());

    Fir fir;
    EXPECT_TRUE(fir.Parse(common_header));
    EXPECT_EQ(kSenderSsrc, fir.sender_ssrc());
    EXPECT_EQ(1u, fir.requests().size());
    EXPECT_EQ(kRemoteSsrc, fir.requests()[0].ssrc);
    EXPECT_EQ(kSeqNr, fir.requests()[0].seq_nr);
}

MY_TEST(RtcpFirTest, Create) {
    Fir fir;
    fir.set_sender_ssrc(kSenderSsrc);
    fir.AddRequest(kRemoteSsrc, kSeqNr);

    BinaryBuffer raw = fir.Build();

    EXPECT_THAT(raw, testing::ElementsAreArray(kPacket));
}
    
} // namespace rtcp
} // namespace naivert 
