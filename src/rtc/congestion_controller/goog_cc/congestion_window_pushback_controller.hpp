#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_CONGESTION_WINDOW_PUSHBACK_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_CONGESTION_WINDOW_PUSHBACK_CONTROLLER_H_

#include "rtc/congestion_controller/base/bwe_defines.hpp"

#include <optional>

namespace naivertc {

// This class enables pushback from congestion window directly to video encoder.
// When the congestion window is filling up, the video encoder target bitrate
// will be reduced accordingly to accomadate the network changes.
// To avoid pausing video too frequently, a minimum encoder target bitrate threshold
// is used to prevent video pause due to a full congestion window.
class CongestionWindwoPushbackController {
public:
    struct Configuration {
        // Indicates if adding pacing to congestion window.
        bool add_pacing = true;
        DataRate min_pushback_bitrate = kDefaultMinPushbackTargetBitrate;
        size_t initial_congestion_window = 0;
    };
    
public:
    CongestionWindwoPushbackController(const Configuration& config);
    ~CongestionWindwoPushbackController();

    void set_congestion_window(size_t congestion_window);

    void OnInflightBytes(int64_t inflight_bytes);
    void OnPacingQueue(int64_t pacing_bytes);

    // Return pushback bitrate based on the target bitrate.
    DataRate AdjustTargetBitrate(DataRate target_bitrate);

private:
    const bool add_pacing_;
    const DataRate min_pushback_bitrate_;

    size_t congestion_window_;
    int64_t inflight_bytes_;
    int64_t pacing_bytes_;
    double encoding_bitrate_ratio_;
};
    
} // namespace naivertc


#endif