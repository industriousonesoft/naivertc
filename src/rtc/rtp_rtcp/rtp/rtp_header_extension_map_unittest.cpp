#include "rtc/rtp_rtcp/rtp/rtp_header_extension_map.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"

#include <gtest/gtest.h>

namespace naivertc {

TEST(RtpHeaderExtensionTest, RegisterByType) {
    RtpHeaderExtensionMap map;
    EXPECT_FALSE(map.IsRegistered(TransmissionOffsetExtension::kType));

    EXPECT_TRUE(map.RegisterByType(3, TransmissionOffsetExtension::kType));

    EXPECT_TRUE(map.IsRegistered(TransmissionOffsetExtension::kType));
    EXPECT_EQ(3, map.GetId(TransmissionOffsetExtension::kType));
    EXPECT_EQ(TransmissionOffsetExtension::kType, map.GetType(3));
}


TEST(RtpHeaderExtensionTest, RegisterByUri) {
    RtpHeaderExtensionMap map;

    EXPECT_TRUE(map.RegisterByUri(3, TransmissionOffsetExtension::kUri));

    EXPECT_TRUE(map.IsRegistered(TransmissionOffsetExtension::kType));
    EXPECT_EQ(3, map.GetId(TransmissionOffsetExtension::kType));
    EXPECT_EQ(TransmissionOffsetExtension::kType, map.GetType(3));
}

TEST(RtpHeaderExtensionTest, RegisterWithTrait) {
    RtpHeaderExtensionMap map;

    EXPECT_TRUE(map.Register<TransmissionOffsetExtension>(3));

    EXPECT_TRUE(map.IsRegistered(TransmissionOffsetExtension::kType));
    EXPECT_EQ(3, map.GetId(TransmissionOffsetExtension::kType));
    EXPECT_EQ(TransmissionOffsetExtension::kType, map.GetType(3));
}

TEST(RtpHeaderExtensionTest, RegisterTwoByteHeaderExtensions) {
    RtpHeaderExtensionMap map;
    // Two-byte header extension needed for id: [15-255].
    EXPECT_TRUE(map.Register<TransmissionOffsetExtension>(18));
    EXPECT_TRUE(map.Register<AbsoluteSendTimeExtension>(255));
}

TEST(RtpHeaderExtensionTest, RegisterIllegalArg) {
    RtpHeaderExtensionMap map;
    // Valid range for id: [1-255].
    EXPECT_FALSE(map.Register<TransmissionOffsetExtension>(0));
    EXPECT_FALSE(map.Register<AbsoluteSendTimeExtension>(256));
}

TEST(RtpHeaderExtensionTest, Idempotent) {
    RtpHeaderExtensionMap map;

    EXPECT_TRUE(map.Register<AbsoluteSendTimeExtension>(3));
    EXPECT_TRUE(map.Register<AbsoluteSendTimeExtension>(3));

    map.Deregister(AbsoluteSendTimeExtension::kType);
    map.Deregister(AbsoluteSendTimeExtension::kType);
}

TEST(RtpHeaderExtensionTest, NonUniqueId) {
    RtpHeaderExtensionMap map;
    EXPECT_TRUE(map.Register<TransmissionOffsetExtension>(3));

    EXPECT_FALSE(map.Register<AbsoluteSendTimeExtension>(3));
    EXPECT_TRUE(map.Register<AbsoluteSendTimeExtension>(4));
}

TEST(RtpHeaderExtensionTest, GetType) {
    RtpHeaderExtensionMap map;
    EXPECT_EQ(RtpHeaderExtensionMap::kInvalidType, map.GetType(3));
    EXPECT_TRUE(map.Register<TransmissionOffsetExtension>(3));

    EXPECT_EQ(TransmissionOffsetExtension::kType, map.GetType(3));
}

TEST(RtpHeaderExtensionTest, GetId) {
    RtpHeaderExtensionMap map;
    EXPECT_EQ(RtpHeaderExtensionMap::kInvalidId,
                map.GetId(TransmissionOffsetExtension::kType));
    EXPECT_TRUE(map.Register<TransmissionOffsetExtension>(3));

    EXPECT_EQ(3, map.GetId(TransmissionOffsetExtension::kType));
}

} // namespace naivertc