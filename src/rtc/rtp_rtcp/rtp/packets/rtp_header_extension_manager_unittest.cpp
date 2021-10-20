#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtp;

namespace naivertc {

TEST(RTP_RTCP_RtpHeaderExtensionTest, RegisterByType) {
    ExtensionManager mgr;
    EXPECT_FALSE(mgr.IsRegistered(TransmissionTimeOffset::kType));

    EXPECT_TRUE(mgr.RegisterByType(3, TransmissionTimeOffset::kType));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}


TEST(RTP_RTCP_RtpHeaderExtensionTest, RegisterByUri) {
    ExtensionManager mgr;

    EXPECT_TRUE(mgr.RegisterByUri(3, TransmissionTimeOffset::kUri));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, RegisterWithTrait) {
    ExtensionManager mgr;

    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionTimeOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, RegisterTwoByteHeaderExtensions) {
    ExtensionManager mgr;
    // Two-byte header extension needed for id: [15-255].
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(18));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(255));
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, RegisterIllegalArg) {
    ExtensionManager mgr;
    // Valid range for id: [1-255].
    EXPECT_FALSE(mgr.Register<TransmissionTimeOffset>(0));
    EXPECT_FALSE(mgr.Register<AbsoluteSendTime>(256));
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, Idempotent) {
    ExtensionManager mgr;

    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(3));

    mgr.Deregister(AbsoluteSendTime::kType);
    mgr.Deregister(AbsoluteSendTime::kType);
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, NonUniqueId) {
    ExtensionManager mgr;
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_FALSE(mgr.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(4));
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, GetType) {
    ExtensionManager mgr;
    EXPECT_EQ(ExtensionManager::kInvalidType, mgr.GetType(3));
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_EQ(TransmissionTimeOffset::kType, mgr.GetType(3));
}

TEST(RTP_RTCP_RtpHeaderExtensionTest, GetId) {
    ExtensionManager mgr;
    EXPECT_EQ(ExtensionManager::kInvalidId,
                mgr.GetId(TransmissionTimeOffset::kType));
    EXPECT_TRUE(mgr.Register<TransmissionTimeOffset>(3));

    EXPECT_EQ(3, mgr.GetId(TransmissionTimeOffset::kType));
}

} // namespace naivertc