#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_map.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::rtp;

namespace naivertc {

MY_TEST(RtpHeaderExtensionTest, RegisterByType) {
    HeaderExtensionMap map;
    EXPECT_FALSE(map.IsRegistered(TransmissionTimeOffset::kType));

    EXPECT_TRUE(map.RegisterByType(3, TransmissionTimeOffset::kType));

    EXPECT_TRUE(map.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, map.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, map.GetType(3));
}


MY_TEST(RtpHeaderExtensionTest, RegisterByUri) {
    HeaderExtensionMap map;

    EXPECT_TRUE(map.RegisterByUri(3, TransmissionTimeOffset::kUri));

    EXPECT_TRUE(map.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, map.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, map.GetType(3));
}

MY_TEST(RtpHeaderExtensionTest, RegisterWithTrait) {
    HeaderExtensionMap map;

    EXPECT_TRUE(map.Register<TransmissionTimeOffset>(3));

    EXPECT_TRUE(map.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, map.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, map.GetType(3));
}

MY_TEST(RtpHeaderExtensionTest, RegisterTwoByteHeaderExtensions) {
    HeaderExtensionMap map;
    // Two-byte header extension needed for id: [15-255].
    EXPECT_TRUE(map.Register<TransmissionTimeOffset>(18));
    EXPECT_TRUE(map.Register<AbsoluteSendTime>(255));
}

MY_TEST(RtpHeaderExtensionTest, RegisterIllegalArg) {
    HeaderExtensionMap map;
    // Valid range for id: [1-255].
    EXPECT_FALSE(map.Register<TransmissionTimeOffset>(0));
    EXPECT_FALSE(map.Register<AbsoluteSendTime>(256));
}

MY_TEST(RtpHeaderExtensionTest, Idempotent) {
    HeaderExtensionMap map;

    EXPECT_TRUE(map.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(map.Register<AbsoluteSendTime>(3));

    map.Deregister(AbsoluteSendTime::kType);
    map.Deregister(AbsoluteSendTime::kType);
}

MY_TEST(RtpHeaderExtensionTest, NonUniqueId) {
    HeaderExtensionMap map;
    EXPECT_TRUE(map.Register<TransmissionTimeOffset>(3));

    EXPECT_FALSE(map.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(map.Register<AbsoluteSendTime>(4));
}

MY_TEST(RtpHeaderExtensionTest, GetType) {
    HeaderExtensionMap map;
    EXPECT_EQ(HeaderExtensionMap::kInvalidType, map.GetType(3));
    EXPECT_TRUE(map.Register<TransmissionTimeOffset>(3));

    EXPECT_EQ(TransmissionTimeOffset::kType, map.GetType(3));
}

MY_TEST(RtpHeaderExtensionTest, GetId) {
    HeaderExtensionMap map;
    EXPECT_EQ(HeaderExtensionMap::kInvalidId,
                map.GetId(TransmissionTimeOffset::kType));
    EXPECT_TRUE(map.Register<TransmissionTimeOffset>(3));

    EXPECT_EQ(3, map.GetId(TransmissionTimeOffset::kType));
}

} // namespace naivertc