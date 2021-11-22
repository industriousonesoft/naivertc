#ifndef _RTC_BASE_SYNCHRONIZATION_YIELD_POLICY_H_
#define _RTC_BASE_SYNCHRONIZATION_YIELD_POLICY_H_

#include "base/defines.hpp"

namespace naivertc {

class RTC_CPP_EXPORT YieldInterface {
public:
    virtual ~YieldInterface() = default;
    virtual void YieldExecution() = 0;
};

// Sets the current thread-local yield policy while it's in scope
// and reverts to the previous thread-local yield policy when it leaves the scope.
class RTC_CPP_EXPORT ScopedYieldPolicy final {
public:
    explicit ScopedYieldPolicy(YieldInterface* policy);
    ScopedYieldPolicy(const ScopedYieldPolicy&) = delete;
    ScopedYieldPolicy& operator=(const ScopedYieldPolicy&) = delete;
    ~ScopedYieldPolicy();

    static void YieldExecution();
  
private:
    // The previous thread-local yield policy.
    YieldInterface* const previous_;
};

} // namespace naivertc

#endif