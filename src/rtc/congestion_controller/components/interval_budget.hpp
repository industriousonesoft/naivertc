#ifndef _RTC_CONGESTION_CONTROLLER_COMPONENTS_INTERVAL_BUDGET_H_
#define _RTC_CONGESTION_CONTROLLER_COMPONENTS_INTERVAL_BUDGET_H_

#include <stddef.h>
#include "rtc/base/units/data_rate.hpp"

namespace naivertc {

class IntervalBudget {
public:
    explicit IntervalBudget(DataRate initial_target_bitrate, 
                            bool can_build_up_underuse = false);
    ~IntervalBudget();

    DataRate target_bitrate() const;
    size_t bytes_remaining() const;
    double budget_ratio() const;

    void set_target_bitrate(DataRate bitrate);

    void IncreaseBudget(int64_t interval_time_ms);
    void ConsumeBudget(size_t bytes);

private:
    DataRate target_bitrate_;
    int64_t max_bytes_in_budget_;
    int64_t bytes_remaining_;
    bool can_build_up_from_underuse_;
};
    
} // namespace naivertc


#endif