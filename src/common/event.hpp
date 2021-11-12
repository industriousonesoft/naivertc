#ifndef _COMMON_EVENT_H_
#define _COMMON_EVENT_H_

#include "base/defines.hpp"

#if defined(NAIVERTC_WIN)
#include <windows.h>
#elif defined(NAIVERTC_POSIX)
#include <pthread.h>
#else
#error "Must define either NAIVERTC_WIN or NAIVERTC_POSIX."
#endif

namespace naivertc {

class RTC_CPP_EXPORT Event {
public:
    static const int kForever = -1;
public:
    Event();
    Event(bool manual_reset, bool initially_signaled);
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    ~Event();

    void Set();
    void Reset();

    // Waits for the event to become signaled, but logs a warning if it takes more
    // than `warn_after_ms` milliseconds, and gives up completely if it takes more
    // than `give_up_after_ms` milliseconds. (If `warn_after_ms >=
    // give_up_after_ms`, no warning will be logged.) Either or both may be
    // `kForever`, which means wait indefinitely.
    //
    // Returns true if the event was signaled, false if there was a timeout or
    // some other error.
    bool Wait(int give_up_after_ms, int warn_after_ms);

    // Waits with the given timeout and a reasonable default warning timeout.
    bool Wait(int give_up_after_ms) {
        return Wait(give_up_after_ms, give_up_after_ms == kForever ? 3000 : kForever);
    }

private:
#if defined(NAIVERTC_WIN)
    HANDLE event_handle_;
#elif defined(NAIVERTC_POSIX)
    pthread_mutex_t event_mutex_;
    pthread_cond_t event_cond_;
    const bool is_manual_reset_;
    bool event_status_;
#endif
};
    
} // namespace naivertc


#endif