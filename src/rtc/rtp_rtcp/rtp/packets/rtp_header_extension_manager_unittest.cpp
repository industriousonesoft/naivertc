#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

using namespace naivertc::rtp;

namespace naivertc {

MY_TEST(RtpHeaderExtensionTest, RegisterByType) {
    ExtensionManager mgr;
    EXPECT_FALSE(mgr.IsRegistered(TransmissionTimeOffset::kType));

    EXPECT_TRUE(mgr.RegisterByType(3, TransmissionTimeOffset::kType));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}


MY_TEST(RtpHeaderExtensionTest, RegisterByUri) {
    ExtensionManager mgr;

    EXPECT_TRUE(mgr.RegisterByUri(3, TransmissionTimeOffset::kUri));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}

MY_TEST(RtpHeaderExtensionTest, RegisterWithTrait) {
    ExtensionManager mgr;

    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}

MY_TEST(RtpHeaderExtensionTest, RegisterTwoByteHeaderExtensions) {
    ExtensionManager mgr;
    // Two-byte header extension needed for id: [15-255].
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(18));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(255));
}

MY_TEST(RtpHeaderExtensionTest, RegisterIllegalArg) {
    ExtensionManager mgr;
    // Valid range for id: [1-255].
    EXPECT_FALSE(mgr.Register<TransmissionTimeOffset>(0));
    EXPECT_FALSE(mgr.Register<AbsoluteSendTime>(256));
}

MY_TEST(RtpHeaderExtensionTest, Idempotent) {
    ExtensionManager mgr;

    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(3));

    mgr.Deregister(AbsoluteSendTime::kType);
    mgr.Deregister(AbsoluteSendTime::kType);
}

MY_TEST(RtpHeaderExtensionTest, NonUniqueId) {
    ExtensionManager mgr;
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_FALSE(mgr.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(4));
}

MY_TEST(RtpHeaderExtensionTest, GetType) {
    ExtensionManager mgr;
    EXPECT_EQ(ExtensionManager::kInvalidType, mgr.GetType(3));
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}

MY_TEST(RtpHeaderExtensionTest, GetId) {
    ExtensionManager mgr;
    EXPECT_EQ(ExtensionManager::kInvalidId,
                mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
}

} // namespace naivertc