#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "testing/simulated_clock.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

// SSRC of remote peer, that sends rtcp packet to the rtcp receiver under test.
constexpr uint32_t kSenderSsrc = 0x10203;
// SSRCs of local peer, that rtcp packet addressed to.
constexpr uint32_t kReceiverMainSsrc = 0x123456;
// RtcpReceiver can accept several ssrc, e.g. regular and rtx streams.
constexpr uint32_t kReceiverExtraSsrc = 0x1234567;
// SSRCs to ignore (i.e. not configured in RtcpReceiver).
constexpr uint32_t kNotToUsSsrc = 0x654321;
constexpr uint32_t kUnknownSenderSsrc = 0x54321;
constexpr int64_t kRtcpIntervalMs = 1000;

class MockRtcpPacketTypeCounterObserver : public RtcpPacketTypeCounterObserver {
public:
    MOCK_METHOD(void, 
                RtcpPacketTypesCounterUpdated, 
                (uint32_t, const RtcpPacketTypeCounter&), 
                (override));
};

class MockRtcpIntraFrameObserver : public RtcpIntraFrameObserver {
public:
    MOCK_METHOD(void, OnReceivedIntraFrameRequest, (uint32_t), (override));
};

class MockRtcpLossNotificationObserver : public RtcpLossNotificationObserver {
public:
    MOCK_METHOD(void,
                OnReceivedLossNotification,
                (uint32_t ssrc,
                uint16_t seq_num_of_last_decodable,
                uint16_t seq_num_of_last_received,
                bool decodability_flag),
                (override));
};

class MockCnameCallbackImpl : public RtcpCnameObserver {
public:
    MOCK_METHOD(void, OnCname, (uint32_t, std::string_view), (override));
};

class MockTransportFeedbackObserver : public RtcpTransportFeedbackObserver {
public:
    MOCK_METHOD(void,
                OnTransportFeedback,
                (const rtcp::TransportFeedback&),
                (override));
};

class MockRtcpReceiverObserver : public RtcpReceiver::Observer {
public:
    MOCK_METHOD(void, OnRequestSendReport, (), (override));
    MOCK_METHOD(void, OnReceivedNack, (const std::vector<uint16_t>&), (override));
    MOCK_METHOD(void,
                OnReceivedRtcpReportBlocks,
                (const std::vector<RtcpReportBlock>&),
                (override));
};
    
} // namespace


struct ReceiverMocks {
    ReceiverMocks() : clock(1'335'900'000) {}

    SimulatedClock clock;

    NiceMock<MockRtcpPacketTypeCounterObserver> packet_type_counter_observer;
    StrictMock<MockRtcpIntraFrameObserver> intra_frame_observer;
    StrictMock<MockRtcpLossNotificationObserver> loss_notification_observer;
    StrictMock<MockTransportFeedbackObserver> transport_feedback_observer;
    StrictMock<MockRtcpReceiverObserver> rtcp_receiver_observer;
};

RtcpConfiguration DefaultConfiguration(ReceiverMocks* mocks) {
    RtcpConfiguration config;

    config.rtcp_report_interval_ms = kRtcpIntervalMs;
    config.local_media_ssrc = kReceiverMainSsrc;
    config.rtx_send_ssrc = kReceiverExtraSsrc;

    config.clock = &mocks->clock;
    config.receiver_only = false;
    config.packet_type_counter_observer = &mocks->packet_type_counter_observer;
    config.intra_frame_observer = &mocks->intra_frame_observer;
    config.loss_notification_observer = &mocks->loss_notification_observer;
    config.transport_feedback_observer = &mocks->transport_feedback_observer;

    return config;
}

MY_TEST(RtcpReceiverTest, BrokenPacketIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);

    const uint8_t bad_packet[] = {0, 0, 0, 0};
    EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated).Times(0);
    receiver.IncomingPacket(bad_packet);
}

MY_TEST(RtcpReceiverTest, InvalidFeedbackIsIgnored) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);

    const uint8_t bad_packet[] = {0x81, rtcp::Rtpfb::kPacketType, 0, 0};
    EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated).Times(0);
    EXPECT_CALL(mocks.transport_feedback_observer, OnTransportFeedback).Times(0);
    receiver.IncomingPacket(bad_packet);
}

MY_TEST(RtcpReceiverTest, InjectSrPacket) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    EXPECT_FALSE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr));

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks(IsEmpty()));
    receiver.IncomingPacket(sr.Build());

    EXPECT_TRUE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr, 
                             nullptr, nullptr, nullptr));
}

MY_TEST(RtcpReceiverTest, InjectSrPacketFromUnknownSender) {
    ReceiverMocks mocks;
    RtcpReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtcp_receiver_observer);
    receiver.set_remote_ssrc(kSenderSsrc);

    int64_t now_ms = mocks.clock.now_ms();
    rtcp::SenderReport sr;
    sr.set_sender_ssrc(kUnknownSenderSsrc);

    EXPECT_CALL(mocks.rtcp_receiver_observer, OnReceivedRtcpReportBlocks);
    receiver.IncomingPacket(sr.Build());

    EXPECT_FALSE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr, 
                             nullptr, nullptr, nullptr));
}
    
} // namespace test
} // namespace naivertc

