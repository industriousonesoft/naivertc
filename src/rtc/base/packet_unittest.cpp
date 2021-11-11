#include "rtc/base/packet.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(PacketTest, BuildPacket) {
    const uint8_t bytes[] = {0x20, 0x30, 0x40, 0x50, 0x60};
    Packet packet(bytes, 5);

    EXPECT_EQ(packet.size(), 5);
    EXPECT_EQ(packet.at(0), 0x20);
    EXPECT_EQ(packet.at(1), 0x30);
}

}
}