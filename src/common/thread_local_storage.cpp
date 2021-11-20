#include "common/thread_local_storage.hpp"
#if !defined(RTC_SUPPORT_THREAD_LOCAL) && defined(RTC_SUPPORT_TLS)
#if defined(NAIVERTC_POSIX)
#include <pthread.h>
#endif // defined(NAIVERTC_POSIX)
#endif // !defined(RTC_SUPPORT_THREAD_LOCAL) && defined(RTC_SUPPORT_TLS)

namespace naivertc {
namespace {

#define UNSUPPORTED_PLATFORM "Platform unsupport the thread-local storage API."
#define UNIMPLEMENTED_PLATFORM "Platform supports TLS but not implement yet"

// Support `thread_local`
#if defined(RTC_SUPPORT_THREAD_LOCAL)

RTC_CONST_INIT thread_local void* g_thread_local_value = nullptr;

// Support TLS
#elif defined(RTC_SUPPORT_TLS)
// POSIX platfrom
#if defined(NAIVERTC_POSIX)
// Emscripten does not support the C++11 thread_local keyword but does support
// the pthread TLS(Thread-local storage) API.
// https://github.com/emscripten-core/emscripten/issues/3502

RTC_CONST_INIT pthread_key_t g_pthread_local_storage_key = 0;

void InitializeTls() {
    assert(pthread_key_create(&g_pthread_local_storage_key, nullptr) == 0);
}

pthread_key_t GetTlsKey() {
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    assert(pthread_once(&init_once, &InitializeTls) == 0);
    return g_pthread_local_storage_key;
}

#else
#error UNIMPLEMENTED_PLATFORM
#endif // defined(NAIVERTC_POSIX)

// Unsupported platform
#else
#error UNSUPPORTED_PLATFORM
#endif // defined(RTC_SUPPORT_THREAD_LOCAL)

} // namespace 

void* ThreadLocalStorage::GetSpecific() {
#if defined(RTC_SUPPORT_THREAD_LOCAL)
    return g_thread_local_value;
#elif defined(RTC_SUPPORT_TLS)

#if defined(NAIVERTC_POSIX)
    return pthread_getspecific(GetTlsKey());
#else
#error UNIMPLEMENTED_PLATFORM
#endif // defined(NAIVERTC_POSIX)

#else
#error UNSUPPORTED_PLATFORM
#endif // defined(RTC_SUPPORT_THREAD_LOCAL)
}

void ThreadLocalStorage::SetSpecific(void* value) {
#if defined(RTC_SUPPORT_THREAD_LOCAL)
    g_thread_local_value = value;
#elif defined(RTC_SUPPORT_TLS)

#if defined(NAIVERTC_POSIX)
    pthread_setspecific(GetTlsKey(), value);
#else
#error UNIMPLEMENTED_PLATFORM
#endif // defined(NAIVERTC_POSIX)

#else
#error UNSUPPORTED_PLATFORM
#endif // defined(RTC_SUPPORT_THREAD_LOCAL)
}

} // namespace naivertc