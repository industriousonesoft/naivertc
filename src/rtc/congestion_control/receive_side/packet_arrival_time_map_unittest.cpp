#include "rtc/congestion_control/receive_side/packet_arrival_time_map.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(PacketArrivalTimeMapTest, IsConsistentWhenEmpty) {
    PacketArrivalTimeMap map;

    EXPECT_EQ(map.begin_packet_id(), map.end_packet_id());
    EXPECT_FALSE(map.HasReceived(0));
    EXPECT_EQ(map.Clamp(-5), 0);
    EXPECT_EQ(map.Clamp(5), 0);
}

MY_TEST(PacketArrivalTimeMapTest, InsertsFirstItemIntoMap) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    EXPECT_EQ(map.begin_packet_id(), 42);
    EXPECT_EQ(map.end_packet_id(), 43);

    EXPECT_FALSE(map.HasReceived(41));
    EXPECT_TRUE(map.HasReceived(42));
    EXPECT_FALSE(map.HasReceived(44));

    EXPECT_EQ(map.Clamp(-100), 42);
    EXPECT_EQ(map.Clamp(42), 42);
    EXPECT_EQ(map.Clamp(100), 43);
}

MY_TEST(PacketArrivalTimeMapTest, InsertsWithGaps) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    map.AddPacket(45, 11);
    EXPECT_EQ(map.begin_packet_id(), 42);
    EXPECT_EQ(map.end_packet_id(), 46);

    EXPECT_FALSE(map.HasReceived(41));
    EXPECT_TRUE(map.HasReceived(42));
    EXPECT_FALSE(map.HasReceived(43));
    EXPECT_FALSE(map.HasReceived(44));
    EXPECT_TRUE(map.HasReceived(45));
    EXPECT_FALSE(map.HasReceived(46));

    EXPECT_EQ(map.at(42), 10);
    EXPECT_EQ(map.at(43), 0);
    EXPECT_EQ(map.at(44), 0);
    EXPECT_EQ(map.at(45), 11);

    EXPECT_EQ(map.Clamp(-100), 42);
    EXPECT_EQ(map.Clamp(44), 44);
    EXPECT_EQ(map.Clamp(100), 46);
}

MY_TEST(PacketArrivalTimeMapTest, InsertsWithinBuffer) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    map.AddPacket(45, 11);

    map.AddPacket(43, 12);
    map.AddPacket(44, 13);

    EXPECT_EQ(map.begin_packet_id(), 42);
    EXPECT_EQ(map.end_packet_id(), 46);

    EXPECT_FALSE(map.HasReceived(41));
    EXPECT_TRUE(map.HasReceived(42));
    EXPECT_TRUE(map.HasReceived(43));
    EXPECT_TRUE(map.HasReceived(44));
    EXPECT_TRUE(map.HasReceived(45));
    EXPECT_FALSE(map.HasReceived(46));

    EXPECT_EQ(map.at(42), 10);
    EXPECT_EQ(map.at(43), 12);
    EXPECT_EQ(map.at(44), 13);
    EXPECT_EQ(map.at(45), 11);
}

MY_TEST(PacketArrivalTimeMapTest, GrowsBufferAndRemoveOld) {
    PacketArrivalTimeMap map;

    constexpr int64_t kLargeSeq = 42 + PacketArrivalTimeMap::kMaxNumberOfPackets;
    map.AddPacket(42, 10);
    map.AddPacket(43, 11);
    map.AddPacket(44, 12);
    map.AddPacket(45, 13);
    map.AddPacket(kLargeSeq, 12);

    EXPECT_EQ(map.begin_packet_id(), 43);
    EXPECT_EQ(map.end_packet_id(), kLargeSeq + 1);
    EXPECT_EQ(static_cast<size_t>(map.end_packet_id() -
                                  map.begin_packet_id()),
                PacketArrivalTimeMap::kMaxNumberOfPackets);

    EXPECT_FALSE(map.HasReceived(41));
    EXPECT_FALSE(map.HasReceived(42));
    EXPECT_TRUE(map.HasReceived(43));
    EXPECT_TRUE(map.HasReceived(44));
    EXPECT_TRUE(map.HasReceived(45));
    EXPECT_FALSE(map.HasReceived(46));
    EXPECT_TRUE(map.HasReceived(kLargeSeq));
    EXPECT_FALSE(map.HasReceived(kLargeSeq + 1));
}

MY_TEST(PacketArrivalTimeMapTest, GrowsBufferAndRemoveOldTrimsBeginning) {
    PacketArrivalTimeMap map;

    constexpr int64_t kLargeSeq = 42 + PacketArrivalTimeMap::kMaxNumberOfPackets;
    map.AddPacket(42, 10);
    // Missing: 43, 44
    map.AddPacket(45, 13);
    map.AddPacket(kLargeSeq, 12);

    EXPECT_EQ(map.begin_packet_id(), 45);
    EXPECT_EQ(map.end_packet_id(), kLargeSeq + 1);

    EXPECT_FALSE(map.HasReceived(44));
    EXPECT_TRUE(map.HasReceived(45));
    EXPECT_FALSE(map.HasReceived(46));
    EXPECT_TRUE(map.HasReceived(kLargeSeq));
    EXPECT_FALSE(map.HasReceived(kLargeSeq + 1));
}

MY_TEST(PacketArrivalTimeMapTest, SequenceNumberJumpsDeletesAll) {
    PacketArrivalTimeMap map;

    constexpr int64_t kLargeSeq =
        42 + 2 * PacketArrivalTimeMap::kMaxNumberOfPackets;
    map.AddPacket(42, 10);
    map.AddPacket(kLargeSeq, 12);

    EXPECT_EQ(map.begin_packet_id(), kLargeSeq);
    EXPECT_EQ(map.end_packet_id(), kLargeSeq + 1);

    EXPECT_FALSE(map.HasReceived(42));
    EXPECT_TRUE(map.HasReceived(kLargeSeq));
    EXPECT_FALSE(map.HasReceived(kLargeSeq + 1));
}

MY_TEST(PacketArrivalTimeMapTest, ExpandsBeforeBeginning) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    map.AddPacket(-1000, 13);

    EXPECT_EQ(map.begin_packet_id(), -1000);
    EXPECT_EQ(map.end_packet_id(), 43);

    EXPECT_FALSE(map.HasReceived(-1001));
    EXPECT_TRUE(map.HasReceived(-1000));
    EXPECT_FALSE(map.HasReceived(-999));
    EXPECT_TRUE(map.HasReceived(42));
    EXPECT_FALSE(map.HasReceived(43));
}

MY_TEST(PacketArrivalTimeMapTest, ExpandingBeforeBeginningKeepsReceived) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    constexpr int64_t kSmallSeq =
        static_cast<int64_t>(42) - 2 * PacketArrivalTimeMap::kMaxNumberOfPackets;
    map.AddPacket(kSmallSeq, 13);

    EXPECT_EQ(map.begin_packet_id(), 42);
    EXPECT_EQ(map.end_packet_id(), 43);
}

MY_TEST(PacketArrivalTimeMapTest, ErasesToRemoveElements) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    map.AddPacket(43, 11);
    map.AddPacket(44, 12);
    map.AddPacket(45, 13);

    map.EraseTo(44);

    EXPECT_EQ(map.begin_packet_id(), 44);
    EXPECT_EQ(map.end_packet_id(), 46);

    EXPECT_FALSE(map.HasReceived(43));
    EXPECT_TRUE(map.HasReceived(44));
    EXPECT_TRUE(map.HasReceived(45));
    EXPECT_FALSE(map.HasReceived(46));
}

MY_TEST(PacketArrivalTimeMapTest, ErasesInEmptyMap) {
    PacketArrivalTimeMap map;

    EXPECT_EQ(map.begin_packet_id(), map.end_packet_id());

    map.EraseTo(map.end_packet_id());
    EXPECT_EQ(map.begin_packet_id(), map.end_packet_id());
}

MY_TEST(PacketArrivalTimeMapTest, IsTolerantToWrongArgumentsForErase) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    map.AddPacket(43, 11);

    map.EraseTo(1);

    EXPECT_EQ(map.begin_packet_id(), 42);
    EXPECT_EQ(map.end_packet_id(), 44);

    map.EraseTo(100);

    EXPECT_EQ(map.begin_packet_id(), 44);
    EXPECT_EQ(map.end_packet_id(), 44);
}

MY_TEST(PacketArrivalTimeMapTest, EraseAllRemembersBeginningSeqNbr) {
    PacketArrivalTimeMap map;

    map.AddPacket(42, 10);
    map.AddPacket(43, 11);
    map.AddPacket(44, 12);
    map.AddPacket(45, 13);

    map.EraseTo(46);

    map.AddPacket(50, 10);

    EXPECT_EQ(map.begin_packet_id(), 46);
    EXPECT_EQ(map.end_packet_id(), 51);

    EXPECT_FALSE(map.HasReceived(45));
    EXPECT_FALSE(map.HasReceived(46));
    EXPECT_FALSE(map.HasReceived(47));
    EXPECT_FALSE(map.HasReceived(48));
    EXPECT_FALSE(map.HasReceived(49));
    EXPECT_TRUE(map.HasReceived(50));
    EXPECT_FALSE(map.HasReceived(51));
}
    
} // namespace test
} // namespace naivertc
