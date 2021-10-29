#include "rtc/rtp_rtcp/rtp/fec/fec_receiver_ulp.hpp"
#include "rtc/base/time/clock_simulated.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace naivertc {
namespace test {
namespace {

using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

constexpr int kFecPayloadType = 96;
constexpr uint32_t kMediaSsrc = 835424;
    
} // namespace

// MockRecoveredPacketReceiver
class MockRecoveredPacketReceiver : public RecoveredPacketReceiver {
public:
    MOCK_METHOD(void, 
                OnRecoveredPacket, 
                (CopyOnWriteBuffer recovered_packet), 
                (override));
};

// UlpFecReceiverTest
class UlpFecReceiverTest : public ::testing::Test {
protected:
    UlpFecReceiverTest() 
        : clock_(std::make_shared<SimulatedClock>(0)),
          ulp_fec_receiver_(std::make_unique<UlpFecReceiver>(kMediaSsrc, clock_, &recovered_packet_receiver_)) {}

protected:
    std::shared_ptr<SimulatedClock> clock_;
    MockRecoveredPacketReceiver recovered_packet_receiver_;
    std::unique_ptr<UlpFecReceiver> ulp_fec_receiver_;
};

} // namespace test
} // namespace naivertc