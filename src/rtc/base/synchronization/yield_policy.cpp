#include "rtc/base/synchronization/yield_policy.hpp"

namespace naivertc {
namespace {

// Support `thread_local`
#if defined(RTC_SUPPORT_THREAD_LOCAL)

RTC_CONST_INIT thread_local YieldInterface* current_yield_policy = nullptr;

YieldInterface* GetCurrentYieldPolicy() {
    return current_yield_policy;
}

void SetCurrentYieldPolicy(YieldInterface* value) {
    current_yield_policy = value;
}

// Support TLS and POSIX platfrom
#elif defined(RTC_SUPPORT_TLS) && defined(NAIVERTC_POSIX)
#include <pthread.h>
// Emscripten does not support the C++11 thread_local keyword but does support
// the pthread TLS(Thread-local storage) API.
// https://github.com/emscripten-core/emscripten/issues/3502

RTC_CONST_INIT pthread_key_t g_current_yield_policy_tls = 0;

void InitializeTls() {
    assert(pthread_key_create(&g_current_yield_policy_tls, nullptr) == 0);
}

pthread_key_t GetTlsKey() {
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    assert(pthread_once(&init_once, &InitializeTls) == 0);
    return g_current_yield_policy_tls;
}

YieldInterface* GetCurrentYieldPolicy() {
    return static_cast<YieldInterface*>(pthread_getspecific(GetTlsKey()));
}

void SetCurrentYieldPolicy(YieldInterface* value) {
    pthread_setspecific(GetTlsKey(), value);
}

// Unsupported platform
#else
#error "Platform unsupport TLS(thread-local storage)."
#endif // defined(RTC_SUPPORT_THREAD_LOCAL)

} // namespace 

ScopedYieldPolicy::ScopedYieldPolicy(YieldInterface* policy) 
    : previous_(GetCurrentYieldPolicy()) {
    SetCurrentYieldPolicy(policy);
}

ScopedYieldPolicy::~ScopedYieldPolicy() {
    // Reverts to the previous thread-local value.
    SetCurrentYieldPolicy(previous_);
}

void ScopedYieldPolicy::YieldExecution() {
    YieldInterface* current = GetCurrentYieldPolicy();
    if (current) {
        current->YieldExecution();
    }
}

} // namespace naivertc