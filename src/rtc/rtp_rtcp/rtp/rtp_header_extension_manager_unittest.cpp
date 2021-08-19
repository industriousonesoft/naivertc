#include "rtc/rtp_rtcp/rtp/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"

#include <gtest/gtest.h>

namespace naivertc {

TEST(RtpHeaderExtensionTest, RegisterByType) {
    RtpHeaderExtensionManager mgr;
    EXPECT_FALSE(mgr.IsRegistered(TransmissionOffsetExtension::kType));

    EXPECT_TRUE(mgr.RegisterByType(3, TransmissionOffsetExtension::kType));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionOffsetExtension::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionOffsetExtension::kType));
    EXPECT_EQ(TransmissionOffsetExtension::kType, mgr.GetType(3));
}


TEST(RtpHeaderExtensionTest, RegisterByUri) {
    RtpHeaderExtensionManager mgr;

    EXPECT_TRUE(mgr.RegisterByUri(3, TransmissionOffsetExtension::kUri));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionOffsetExtension::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionOffsetExtension::kType));
    EXPECT_EQ(TransmissionOffsetExtension::kType, mgr.GetType(3));
}

TEST(RtpHeaderExtensionTest, RegisterWithTrait) {
    RtpHeaderExtensionManager mgr;

    EXPECT_TRUE(mgr.Register<TransmissionOffsetExtension>(3));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionOffsetExtension::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionOffsetExtension::kType));
    EXPECT_EQ(TransmissionOffsetExtension::kType, mgr.GetType(3));
}

TEST(RtpHeaderExtensionTest, RegisterTwoByteHeaderExtensions) {
    RtpHeaderExtensionManager mgr;
    // Two-byte header extension needed for id: [15-255].
    EXPECT_TRUE(mgr.Register<TransmissionOffsetExtension>(18));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTimeExtension>(255));
}

TEST(RtpHeaderExtensionTest, RegisterIllegalArg) {
    RtpHeaderExtensionManager mgr;
    // Valid range for id: [1-255].
    EXPECT_FALSE(mgr.Register<TransmissionOffsetExtension>(0));
    EXPECT_FALSE(mgr.Register<AbsoluteSendTimeExtension>(256));
}

TEST(RtpHeaderExtensionTest, Idempotent) {
    RtpHeaderExtensionManager mgr;

    EXPECT_TRUE(mgr.Register<AbsoluteSendTimeExtension>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTimeExtension>(3));

    mgr.Deregister(AbsoluteSendTimeExtension::kType);
    mgr.Deregister(AbsoluteSendTimeExtension::kType);
}

TEST(RtpHeaderExtensionTest, NonUniqueId) {
    RtpHeaderExtensionManager mgr;
    EXPECT_TRUE(mgr.Register<TransmissionOffsetExtension>(3));

    EXPECT_FALSE(mgr.Register<AbsoluteSendTimeExtension>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTimeExtension>(4));
}

TEST(RtpHeaderExtensionTest, GetType) {
    RtpHeaderExtensionManager mgr;
    EXPECT_EQ(RtpHeaderExtensionManager::kInvalidType, mgr.GetType(3));
    EXPECT_TRUE(mgr.Register<TransmissionOffsetExtension>(3));

    EXPECT_EQ(TransmissionOffsetExtension::kType, mgr.GetType(3));
}

TEST(RtpHeaderExtensionTest, GetId) {
    RtpHeaderExtensionManager mgr;
    EXPECT_EQ(RtpHeaderExtensionManager::kInvalidId,
                mgr.GetId(TransmissionOffsetExtension::kType));
    EXPECT_TRUE(mgr.Register<TransmissionOffsetExtension>(3));

    EXPECT_EQ(3, mgr.GetId(TransmissionOffsetExtension::kType));
}

} // namespace naivertc