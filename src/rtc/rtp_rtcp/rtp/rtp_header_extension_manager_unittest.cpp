#include "rtc/rtp_rtcp/rtp/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtp::extension;

namespace naivertc {

TEST(RtpHeaderExtensionTest, RegisterByType) {
    Manager mgr;
    EXPECT_FALSE(mgr.IsRegistered(TransmissionOffset::kType));

    EXPECT_TRUE(mgr.RegisterByType(3, TransmissionOffset::kType));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionOffset::kType));
    EXPECT_EQ(TransmissionOffset::kType, mgr.GetType(3));
}


TEST(RtpHeaderExtensionTest, RegisterByUri) {
    Manager mgr;

    EXPECT_TRUE(mgr.RegisterByUri(3, TransmissionOffset::kUri));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionOffset::kType));
    EXPECT_EQ(TransmissionOffset::kType, mgr.GetType(3));
}

TEST(RtpHeaderExtensionTest, RegisterWithTrait) {
    Manager mgr;

    EXPECT_TRUE(mgr.Register<TransmissionOffset>(3));

    EXPECT_TRUE(mgr.IsRegistered(TransmissionOffset::kType));
    EXPECT_EQ(3, mgr.GetId(TransmissionOffset::kType));
    EXPECT_EQ(TransmissionOffset::kType, mgr.GetType(3));
}

TEST(RtpHeaderExtensionTest, RegisterTwoByteHeaderExtensions) {
    Manager mgr;
    // Two-byte header extension needed for id: [15-255].
    EXPECT_TRUE(mgr.Register<TransmissionOffset>(18));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(255));
}

TEST(RtpHeaderExtensionTest, RegisterIllegalArg) {
    Manager mgr;
    // Valid range for id: [1-255].
    EXPECT_FALSE(mgr.Register<TransmissionOffset>(0));
    EXPECT_FALSE(mgr.Register<AbsoluteSendTime>(256));
}

TEST(RtpHeaderExtensionTest, Idempotent) {
    Manager mgr;

    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(3));

    mgr.Deregister(AbsoluteSendTime::kType);
    mgr.Deregister(AbsoluteSendTime::kType);
}

TEST(RtpHeaderExtensionTest, NonUniqueId) {
    Manager mgr;
    EXPECT_TRUE(mgr.Register<TransmissionOffset>(3));

    EXPECT_FALSE(mgr.Register<AbsoluteSendTime>(3));
    EXPECT_TRUE(mgr.Register<AbsoluteSendTime>(4));
}

TEST(RtpHeaderExtensionTest, GetType) {
    Manager mgr;
    EXPECT_EQ(Manager::kInvalidType, mgr.GetType(3));
    EXPECT_TRUE(mgr.Register<TransmissionOffset>(3));

    EXPECT_EQ(TransmissionOffset::kType, mgr.GetType(3));
}

TEST(RtpHeaderExtensionTest, GetId) {
    Manager mgr;
    EXPECT_EQ(Manager::kInvalidId,
                mgr.GetId(TransmissionOffset::kType));
    EXPECT_TRUE(mgr.Register<TransmissionOffset>(3));

    EXPECT_EQ(3, mgr.GetId(TransmissionOffset::kType));
}

} // namespace naivertc