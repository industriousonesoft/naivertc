#include "testing/simulated_task_queue.hpp"
#include "testing/simulated_time_controller.hpp"

namespace naivertc {
 
SimulatedTaskQueue::SimulatedTaskQueue(SimulatedTimeController* handler) 
    : handler_(handler) {
    handler->Register(this);
}

SimulatedTaskQueue::~SimulatedTaskQueue() {
    handler_->Deregister(this);
}

// Provides next run time.
Timestamp SimulatedTaskQueue::GetNextRunTime() const {
    std::lock_guard lock(lock_);
    return next_run_time_;
}

// Runs all ready tasks next run time.
void SimulatedTaskQueue::RunReady(Timestamp at_time) {
    std::lock_guard lock(lock_);
    for (auto it = delayed_tasks_.begin(); 
         it != delayed_tasks_.end() && it->first <= at_time;
         it = delayed_tasks_.erase(it)) {
        for (auto& task : it->second) {
            ready_tasks_.push_back(std::move(task));
        }
    }
    CurrentTaskQueueSetter set_current(this);
    while (!ready_tasks_.empty()) {
        auto ready = std::move(ready_tasks_.front());
        ready_tasks_.pop_front();
        // NOTE: In case of the `Run` function of `ready`
        // might indirectly cause a call to the public functions
        // of SimulatedTaskQueue instance, which will grab `lock_` again,
        // we should make sure the `lock_` is free before calling `Run`.
        lock_.unlock();
        ready();
        lock_.lock();
    }
    if (!delayed_tasks_.empty()) {
        next_run_time_ = delayed_tasks_.begin()->first;
    } else {
        next_run_time_ = Timestamp::PlusInfinity();
    }
}

void SimulatedTaskQueue::Post(std::function<void()> handler) {
    std::lock_guard lock(lock_);
    ready_tasks_.emplace_back(std::move(handler));
    // Run the task ASAP.
    next_run_time_ = Timestamp::MinusInfinity();
}

void SimulatedTaskQueue::PostDelayed(TimeDelta delay, std::function<void()> handler) {
    std::lock_guard lock(lock_);
    Timestamp target_time = handler_->CurrentTime() + delay;
    delayed_tasks_[target_time].push_back(std::move(handler));
    next_run_time_ = std::min(next_run_time_, target_time);
}

void SimulatedTaskQueue::Delete() {
    // Need to destroy the tasks outside of the lock because task destruction
    // can lead to re-entry in SimulatedTaskQueue via custom destructors.
    // FIXME: Why we need to prevent re-entry here?
    ReadyTaskDeque ready_tasks;
    DelayedTaskMap delayed_tasks;
    {
        std::lock_guard lock(lock_);
        ready_tasks_.swap(ready_tasks);
        delayed_tasks_.swap(delayed_tasks);
    }
    ready_tasks.clear();
    delayed_tasks.clear();
    delete this;
}

} // namespace naivertc