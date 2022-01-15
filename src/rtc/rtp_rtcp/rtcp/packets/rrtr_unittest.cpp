#include "rtc/rtp_rtcp/rtcp/packets/rrtr.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

const uint32_t kNtpSec = 0x12345678;
const uint32_t kNtpFrac = 0x23456789;
const uint8_t kBlock[] = {0x04, 0x00, 0x00, 0x02, 0x12, 0x34,
                         0x56, 0x78, 0x23, 0x45, 0x67, 0x89};
const size_t kBlockSize = sizeof(kBlock);

MY_TEST(RtcpPacketRrtrTest, PackInto) {
    const size_t kBufferSize = 12;
    uint8_t buffer[kBufferSize];
    Rrtr rrtr;
    rrtr.set_ntp(NtpTime(kNtpSec, kNtpFrac));

    rrtr.PackInto(buffer, kBufferSize);
    EXPECT_EQ(0, memcmp(buffer, kBlock, kBlockSize));
}

MY_TEST(RtcpPacketRrtrTest, Parse) {
    Rrtr rrtr;
    rrtr.Parse(kBlock, kBlockSize);

    EXPECT_EQ(kNtpSec, rrtr.ntp().seconds());
    EXPECT_EQ(kNtpFrac, rrtr.ntp().fractions());
}
    
} // namespace test
} // namespace naivertc
