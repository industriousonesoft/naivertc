#include "rtc/base/packet.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(PacketTest, BuildPacket) {
    const uint8_t bytes[] = {0x20, 0x30, 0x40, 0x50, 0x60};
    auto packet = Packet::Create(bytes, 5);

    EXPECT_EQ(packet->size(), 5);
    EXPECT_EQ(packet->front(), 0x20);
    EXPECT_EQ(packet->data()[1], 0x30);
}

}
}