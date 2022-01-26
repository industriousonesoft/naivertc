#include "rtc/rtp_rtcp/rtp_sender.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

using namespace ::testing;

namespace naivertc {
namespace test {
namespace {

const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint32_t kRtxSsrc = 12345;
const uint32_t kFlexFecSsrc = 45678;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};
const int64_t kDefaultExpectedRetransmissionTimeMs = 125;
const size_t kMaxPaddingLength = 224;      // Value taken from rtp_sender.cc.
const uint32_t kTimestampTicksPerMs = 90;  // 90kHz clock.
    
} // namespace


    
} // namespace test
} // namespace naivert 
