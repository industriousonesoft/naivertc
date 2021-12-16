#include "rtc/congestion_controller/goog_cc/send_side_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/defines.hpp"

namespace naivertc {
namespace {

constexpr TimeDelta kBweIncreaseInterval = TimeDelta::Millis(1000);
constexpr TimeDelta kBweDecreaseInterval = TimeDelta::Millis(300);
constexpr TimeDelta kStartPhase = TimeDelta::Millis(2000);
constexpr TimeDelta kBweConverganceTime = TimeDelta::Millis(20000);
constexpr int kLimitNumPackets = 20;
constexpr DataRate kDefaultMaxBitrate = DataRate::BitsPerSec(1000000000);
constexpr TimeDelta kLowBitrateLogPeriod = TimeDelta::Millis(10000);
constexpr TimeDelta kRtcEventLogPeriod = TimeDelta::Millis(5000);
// Expecting that RTCP feedback is sent uniformly within [0.5, 1.5]s intervals.
constexpr TimeDelta kMaxRtcpFeedbackInterval = TimeDelta::Millis(5000);

constexpr float kDefaultLowLossThreshold = 0.02f;
constexpr float kDefaultHighLossThreshold = 0.1f;
constexpr DataRate kDefaultBitrateThreshold = DataRate::Zero();

struct UmaRampUpMetric {
    const char* metric_name;
    int bitrate_kbps;
};

const UmaRampUpMetric kUmaRampupMetrics[] = {
    {"NaivrRTC.BWE.RampUpTimeTo500kbpsInMs", 500},
    {"NaivrRTC.BWE.RampUpTimeTo1000kbpsInMs", 1000},
    {"NaivrRTC.BWE.RampUpTimeTo2000kbpsInMs", 2000}};
const size_t kNumUmaRampupMetrics = sizeof(kUmaRampupMetrics) / sizeof(kUmaRampupMetrics[0]);
    
} // namespace


SendSideBwe::SendSideBwe() 
    : rtt_backoff_({}), 
      linker_capacity_tracker_(),
      lost_packtes_since_last_loss_update_(0),
      expected_packets_since_last_loss_update_(0),
      curr_target_bitrate_(DataRate::Zero()),
      min_configured_bitrate_(DataRate::BitsPerSec(kMinBitrateBps)),
      max_configured_bitrate_(kDefaultMaxBitrate),
      has_decreased_since_last_fraction_loss_(false),
      time_last_loss_feedback_(Timestamp::MinusInfinity()),
      time_last_loss_packet_report_(Timestamp::MinusInfinity()),
      last_fraction_loss_(0),
      last_rtt_(TimeDelta::Zero()),
      remb_limit_(DataRate::PlusInfinity()),
      remb_limit_cpas_only_(true),
      delay_based_limit_(DataRate::PlusInfinity()),
      time_last_decrease_(Timestamp::MinusInfinity()),
      time_first_report_(Timestamp::MinusInfinity()),
      initially_loss_packets_(0),
      bitrate_at_2_seconds_(DataRate::Zero()),
      uma_update_state_(NO_UPDATE),
      uam_rtt_state_(NO_UPDATE),
      rampup_uma_states_updated_(kNumUmaRampupMetrics, false),
      low_loss_threshold_(kDefaultLowLossThreshold),
      high_loss_threshold_(kDefaultHighLossThreshold),
      bitrate_threshold_(kDefaultBitrateThreshold),
      loss_based_bwe_({}) {}

SendSideBwe::~SendSideBwe() = default;
    
} // namespace naivertc
