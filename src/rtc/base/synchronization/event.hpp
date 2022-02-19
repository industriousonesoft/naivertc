#ifndef _RTC_BASE_SYNCHRONIZATION_EVENT_H_
#define _RTC_BASE_SYNCHRONIZATION_EVENT_H_

#include "base/defines.hpp"

#if defined(NAIVERTC_WIN)
#include <windows.h>
#elif defined(NAIVERTC_POSIX)
#include <pthread.h>
#else
#error "Must define either NAIVERTC_WIN or NAIVERTC_POSIX."
#endif

namespace naivertc {

class Event { 
public:
    Event();
    Event(bool manual_reset, bool initially_signaled);
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    ~Event();

    void Set();
    void Reset();

    // Returns true if the event was signaled, false if there was a timeout or
    // some other error.
    bool Wait(int give_up_after_ms, int warn_after_ms);

    // Waits with the given timeout and a reasonable default warning timeout.
    bool Wait(int give_up_after_ms) {
        return Wait(give_up_after_ms, give_up_after_ms == kForever ? 3000 : kForever);
    }

    bool WaitForever() {
        return Wait(kForever);
    }

private:
    static const int kForever = -1;
#if defined(NAIVERTC_WIN)
    HANDLE event_handle_;
#elif defined(NAIVERTC_POSIX)
    pthread_mutex_t event_mutex_;
    pthread_cond_t event_cond_;
    const bool is_manual_reset_;
    bool stop_waiting_;
#endif
};
    
} // namespace naivertc


#endif