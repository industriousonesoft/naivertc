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
    : current_time_(start_time) {}

SimulatedTimeController::~SimulatedTimeController() = default;

Timestamp SimulatedTimeController::CurrentTime() const {
    std::lock_guard lock(time_lock_);
    return current_time_;
}

void SimulatedTimeController::AdvanceTimeTo(Timestamp target_time) {
    std::lock_guard lock(time_lock_);
    assert(target_time >= current_time_);
    current_time_ = target_time;
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
    
} // namespace naivertc
