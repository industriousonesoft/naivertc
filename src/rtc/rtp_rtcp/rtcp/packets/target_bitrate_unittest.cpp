#include "rtc/rtp_rtcp/rtcp/packets/target_bitrate.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {
namespace {

constexpr uint32_t kSsrc = 0x12345678;

// clang-format off
const uint8_t kPacket[] = {TargetBitrate::kBlockType,  // Block ID.
                                  0x00,                 // Reserved.
                                        0x00, 0x04,     // Length = 4 words.
                            0x00, 0x01, 0x02, 0x03,     // S0T0 0x010203 kbps.
                            0x01, 0x02, 0x03, 0x04,     // S0T1 0x020304 kbps.
                            0x10, 0x03, 0x04, 0x05,     // S1T0 0x030405 kbps.
                            0x11, 0x04, 0x05, 0x06 };   // S1T1 0x040506 kbps.
constexpr size_t kPacketSize = sizeof(kPacket);
// clang-format on

void Verify(const TargetBitrate::BitrateItem& expected, 
            const TargetBitrate::BitrateItem& actual) {
    EXPECT_EQ(expected.spatial_layer, actual.spatial_layer);
    EXPECT_EQ(expected.temporal_layer, actual.temporal_layer);
    EXPECT_EQ(expected.target_bitrate_kbps, actual.target_bitrate_kbps);
}

void Verify(const std::vector<TargetBitrate::BitrateItem>& items) {
    EXPECT_EQ(4, items.size());
    Verify(TargetBitrate::BitrateItem(0, 0, 0x010203), items[0]);
    Verify(TargetBitrate::BitrateItem(0, 1, 0x020304), items[1]);
    Verify(TargetBitrate::BitrateItem(1, 0, 0x030405), items[2]);
    Verify(TargetBitrate::BitrateItem(1, 1, 0x040506), items[3]);
}
    
} // namespace

MY_TEST(TargetBitrateTest, Parse) {
    TargetBitrate target_bitrate;
    target_bitrate.Parse(kPacket, kPacketSize);
    Verify(target_bitrate.GetTargetBitrates());
}

// TEST(TargetBitrateTest, FullPacket) {
//   const size_t kXRHeaderSize = 8;  // RTCP header (4) + SSRC (4).
//   const size_t kTotalSize = kXRHeaderSize + sizeof(kPacket);
//   uint8_t kRtcpPacket[kTotalSize] = {2 << 6, 207,  0x00, (kTotalSize / 4) - 1,
//                                      0x12,   0x34, 0x56, 0x78};  // SSRC.
//   memcpy(&kRtcpPacket[kXRHeaderSize], kPacket, sizeof(kPacket));
//   rtcp::ExtendedReports xr;
//   EXPECT_TRUE(ParseSinglePacket(kRtcpPacket, &xr));
//   EXPECT_EQ(kSsrc, xr.sender_ssrc());
//   const absl::optional<TargetBitrate>& target_bitrate = xr.target_bitrate();
//   ASSERT_TRUE(static_cast<bool>(target_bitrate));
//   CheckBitrateItems(target_bitrate->GetTargetBitrates());
// }

MY_TEST(TargetBitrateTest, Create) {
    TargetBitrate target_bitrate;
    target_bitrate.AddTargetBitrate(0, 0, 0x010203);
    target_bitrate.AddTargetBitrate(0, 1, 0x020304);
    target_bitrate.AddTargetBitrate(1, 0, 0x030405);
    target_bitrate.AddTargetBitrate(1, 1, 0x040506);

    uint8_t buffer[sizeof(kPacket)] = {};
    ASSERT_EQ(sizeof(kPacket), target_bitrate.BlockSize());
    target_bitrate.PackInto(buffer, kPacketSize);

    EXPECT_EQ(0, memcmp(kPacket, buffer, sizeof(kPacket)));
}

MY_TEST(TargetBitrateTest, ParseNullBitratePacket) {
    const uint8_t kNullPacket[] = {TargetBitrate::kBlockType, 0x00, 0x00, 0x00};
    TargetBitrate target_bitrate;
    target_bitrate.Parse(kNullPacket, 0);
    EXPECT_TRUE(target_bitrate.GetTargetBitrates().empty());
}

} // namespace test
} // namespace naivertc
