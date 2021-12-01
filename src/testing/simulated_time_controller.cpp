#include "testing/simulated_time_controller.hpp"

namespace naivertc {
namespace {
// Helper funciton to remove from a std container by value
template <class C>
bool RemoveByValue(C* container, typename C::value_type val) {
    auto it = std::find(container->begin(), container->end(), val);
    if (it == container->end()) {
        return false;
    }
    container->erase(it);
    return true;
}
    
} // namespace

SimulatedTimeController::SimulatedTimeController(Timestamp start_time) 
    : thread_id_(CurrentThreadId()), 
      current_time_(start_time),
      sim_clock_(std::make_shared<SimulatedClock>(start_time.us())),
      yield_policy_(this) {}

SimulatedTimeController::~SimulatedTimeController() = default;

std::unique_ptr<SimulatedTaskQueue, SimulatedTaskQueue::Deleter> SimulatedTimeController::CreateTaskQueue() {
    return std::unique_ptr<SimulatedTaskQueue, SimulatedTaskQueue::Deleter>(new SimulatedTaskQueue(this));
}

Clock* SimulatedTimeController::Clock() const {
    return sim_clock_.get();
}

Timestamp SimulatedTimeController::CurrentTime() const {
    std::lock_guard lock(time_lock_);
    return current_time_;
}

Timestamp SimulatedTimeController::NextRunTime() const {
    Timestamp curr_time = CurrentTime();
    Timestamp next_time = Timestamp::PlusInfinity();
    std::lock_guard lock(lock_);
    for (auto* runner : runners_) {
        Timestamp next_run_time = runner->GetNextRunTime();
        if (next_run_time <= curr_time) {
            return curr_time;
        }
        next_time = std::min(next_time, next_run_time);
    }
    return next_time;
}

void SimulatedTimeController::AdvanceTime(TimeDelta duration) {
    Timestamp curr_time = CurrentTime();
    Timestamp target_time = curr_time + duration;
    while (curr_time < target_time) {
        RunReadyRunners();
        Timestamp next_time = std::min(NextRunTime(), target_time);
        AdvanceTimeTo(next_time);
        auto delta = next_time - curr_time;
        sim_clock_->AdvanceTime(delta);
        curr_time = next_time;
    }
    // After time has been simulated up until `target_time` we also need to run
    // tasks meant to be executed at `target_time`.
    RunReadyRunners();
}

void SimulatedTimeController::Register(SimulatedSequenceRunner* runner) {
    std::lock_guard<std::mutex> lock(lock_);
    runners_.push_back(runner);
}

void SimulatedTimeController::Deregister(SimulatedSequenceRunner* runner) {
    std::lock_guard lock(lock_);
    bool removed = RemoveByValue(&runners_, runner);
    if (removed) {
        RemoveByValue(&ready_runners_, runner);
    }
}

void SimulatedTimeController::YieldExecution() {
    if (CurrentThreadId() == thread_id_) {
        TaskQueueImpl* yielding_from = TaskQueueImpl::Current();
        // Since we might continue execution on a process thread, we should reset
        // the thread local task queue reference. This ensures that thread checkers
        // won't think we are executing on the yielding task queue. It also ensure
        // that TaskQueueBase::Current() won't return the yielding task queue.
        // NOTE: The current task queue of this thread will reset to `yielding_from`
        // at the end of the call.
        TokenTaskQueue::CurrentTaskQueueSetter reset_queue(nullptr);
        // When we yield, we don't want to risk executing further tasks on the
        // currently executing task queue. If there's a ready task that also yields,
        // it's added to this set as well and only tasks on the remaining task
        // queues are executed.
        auto inserted = yielded_runners_.insert(yielding_from);
        assert(inserted.second);
        RunReadyRunners();
        yielded_runners_.erase(inserted.first);
    }
}

// Private methods
void SimulatedTimeController::AdvanceTimeTo(Timestamp target_time) {
    std::lock_guard lock(time_lock_);
    assert(target_time >= current_time_);
    current_time_ = target_time;
}

void SimulatedTimeController::RunReadyRunners() {
    std::lock_guard lock(lock_);
    assert(CurrentThreadId() == thread_id_);
    Timestamp curr_time = CurrentTime();
    ready_runners_.clear();

    while (true) {
        for (auto* runner : runners_) {
            if (runner->GetNextRunTime() <= curr_time) {
                ready_runners_.push_back(runner);
            }
        }
        if (ready_runners_.empty()) {
            break;
        }
        while (!ready_runners_.empty()) {
            auto* runner = ready_runners_.front();
            ready_runners_.pop_front();
            lock_.unlock();
            // NOTE: As `RunReady()` might indirectly cause a call to
            // `Deregister()` which will grab `lock_` again to remove
            // items from `ready_runners_`, we should make sure the `lock_`
            // is free before calling `RunReady()`.
            runner->RunReady(curr_time);
            lock_.lock();
        }
    }
}
    
} // namespace naivertc
