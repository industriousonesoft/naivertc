#include "rtc/congestion_controller/goog_cc/acknowledged_bitrate_estimator.hpp"
#include "rtc/congestion_controller/goog_cc/bitrate_estimator.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;

namespace naivertc {
namespace test {
namespace {

constexpr int64_t kFirstArrivalTimeMs = 10;
constexpr int64_t kFirstSendTimeMs = 10;
constexpr uint16_t kSequenceNumber = 1;
constexpr size_t kPayloadSize = 10;

class MockBitrateEstimator : public BitrateEstimator {
public:
    // Inherits the constructs from superclass.
    using BitrateEstimator::BitrateEstimator;
    MOCK_METHOD(void, Update, (Timestamp at_time, size_t amount, bool in_alr), (override));
    MOCK_METHOD(std::optional<DataRate>, Estimate, (), (const, override));
    MOCK_METHOD(std::optional<DataRate>, PeekRate, (), (const, override));
    MOCK_METHOD(void, ExpectFastRateChange, (), (override));
};

struct AcknowledgedBitrateEstimatorTestStates {
    std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator;
    MockBitrateEstimator* bitrate_estimator;
};

AcknowledgedBitrateEstimatorTestStates CreateTestStates() {
    AcknowledgedBitrateEstimatorTestStates states;
    auto mock_bitrate_estimator = std::make_unique<MockBitrateEstimator>(MockBitrateEstimator::Configuration());
    states.bitrate_estimator = mock_bitrate_estimator.get();
    states.acknowledged_bitrate_estimator = std::make_unique<AcknowledgedBitrateEstimator>(std::move(mock_bitrate_estimator));
    return states;
}

std::vector<PacketResult> CreateFeedbackVector() {
    std::vector<PacketResult> packet_feedback_vector(2);
    // The first packet feedback.
    packet_feedback_vector[0].recv_time =
      Timestamp::Millis(kFirstArrivalTimeMs);
    packet_feedback_vector[0].sent_packet.send_time =
        Timestamp::Millis(kFirstSendTimeMs);
    packet_feedback_vector[0].sent_packet.sequence_number = kSequenceNumber;
    packet_feedback_vector[0].sent_packet.size = kPayloadSize;
    // The second packet feedback.
    packet_feedback_vector[1].recv_time =
        Timestamp::Millis(kFirstArrivalTimeMs + 10);
    packet_feedback_vector[1].sent_packet.send_time =
        Timestamp::Millis(kFirstSendTimeMs + 10);
    packet_feedback_vector[1].sent_packet.sequence_number = kSequenceNumber + 1;
    packet_feedback_vector[1].sent_packet.size = kPayloadSize + 10;
    return packet_feedback_vector;
}
    
} // namespace

MY_TEST(AcknowledgedBitrateEstimatorTest, UpdateBandwidth) {
    auto states = CreateTestStates();
    auto packet_feedback_vector = CreateFeedbackVector();
    {
        InSequence dummy;
        EXPECT_CALL(*states.bitrate_estimator, Update(packet_feedback_vector[0].recv_time,
                                                      packet_feedback_vector[0].sent_packet.size,
                                                      /* in_alr */ false)).Times(1);
        EXPECT_CALL(*states.bitrate_estimator, Update(packet_feedback_vector[1].recv_time,
                                                      packet_feedback_vector[1].sent_packet.size,
                                                      /* in_alr */ false)).Times(1);
    }
    states.acknowledged_bitrate_estimator->IncomingPacketFeedbacks(packet_feedback_vector);
}

MY_TEST(TestAcknowledgedBitrateEstimator, ExpectFastRateChangeWhenLeftAlr) {
  auto states = CreateTestStates();
  auto packet_feedback_vector = CreateFeedbackVector();
  {
    InSequence dummy;
    EXPECT_CALL(*states.bitrate_estimator,
                Update(packet_feedback_vector[0].recv_time,
                       packet_feedback_vector[0].sent_packet.size,
                       /*in_alr*/ false)).Times(1);
    EXPECT_CALL(*states.bitrate_estimator, ExpectFastRateChange()).Times(1);
    EXPECT_CALL(*states.bitrate_estimator,
                Update(packet_feedback_vector[1].recv_time,
                       packet_feedback_vector[1].sent_packet.size,
                       /*in_alr*/ false)).Times(1);
  }
  states.acknowledged_bitrate_estimator->set_alr_ended_time(Timestamp::Millis(kFirstArrivalTimeMs + 1));
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbacks(packet_feedback_vector);
}

MY_TEST(TestAcknowledgedBitrateEstimator, ReturnBitrate) {
  auto states = CreateTestStates();
  std::optional<DataRate> return_value = DataRate::KilobitsPerSec(123);
  EXPECT_CALL(*states.bitrate_estimator, Estimate()).Times(1)
                                                    .WillOnce(Return(return_value));
  EXPECT_EQ(return_value, states.acknowledged_bitrate_estimator->Estimate());
}

} // namespace test
} // namespace naivertc
