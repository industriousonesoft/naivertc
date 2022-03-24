#include "rtc/base/task_utils/pending_task_safety_flag.hpp"

namespace naivertc {

std::shared_ptr<PendingTaskSafetyFlag> PendingTaskSafetyFlag::Create() {
    return std::shared_ptr<PendingTaskSafetyFlag>(new PendingTaskSafetyFlag(true));
}

std::shared_ptr<PendingTaskSafetyFlag> PendingTaskSafetyFlag::CreateDetached() {
    auto safety_flag = std::shared_ptr<PendingTaskSafetyFlag>(new PendingTaskSafetyFlag(true));
    safety_flag->sequnce_checker_.Detach();
    return safety_flag;
}

bool PendingTaskSafetyFlag::alive() const {
    RTC_RUN_ON(&sequnce_checker_);
    return alive_;
}

void PendingTaskSafetyFlag::SetAlive() {
    RTC_RUN_ON(&sequnce_checker_);
    alive_ = true;
}

void PendingTaskSafetyFlag::SetNotAlive() {
    RTC_RUN_ON(&sequnce_checker_);
    alive_ = false;
}


} // namespace naivertc