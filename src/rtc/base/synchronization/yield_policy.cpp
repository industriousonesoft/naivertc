#include "rtc/base/synchronization/yield_policy.hpp"
#include "common/thread_local_storage.hpp"

namespace naivertc {

ScopedYieldPolicy::ScopedYieldPolicy(YieldInterface* policy) 
    : previous_(ThreadLocalStorage::GetSpecific()) {
    ThreadLocalStorage::SetSpecific(policy);
}

ScopedYieldPolicy::~ScopedYieldPolicy() {
    // Reverts to the previous thread-local value.
    ThreadLocalStorage::SetSpecific(previous_);
}

void ScopedYieldPolicy::YieldExecution() {
    YieldInterface* current = static_cast<YieldInterface*>(ThreadLocalStorage::GetSpecific());
    if (current) {
        current->YieldExecution();
    }
}

} // namespace naivertc