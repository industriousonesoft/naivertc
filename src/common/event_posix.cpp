#include "common/event.hpp"

#include <plog/Log.h>

#include <optional>

#if defined(NAIVERTC_POSIX)

#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

// On MacOS, clock_gettime is available from version 10.12, and on
// iOS, from version 10.0. So we can't use it yet.
#if defined(NAIVERTC_MAC) || defined(NAIVERTC_IOS)
#define USE_CLOCK_GETTIME 0
#define USE_PTHREAD_COND_TIMEDWAIT_MONOTONIC_NP 0
// On Android, pthread_condattr_setclock is available from version 21. By
// default, we target a new enough version for 64-bit platforms but not for
// 32-bit platforms. For older versions, use
// pthread_cond_timedwait_monotonic_np.
#elif defined(NAIVERTC_ANDROID) && (__ANDROID_API__ < 21)
#define USE_CLOCK_GETTIME 1
#define USE_PTHREAD_COND_TIMEDWAIT_MONOTONIC_NP 1
#else
#define USE_CLOCK_GETTIME 1
#define USE_PTHREAD_COND_TIMEDWAIT_MONOTONIC_NP 0
#endif

namespace naivertc {
namespace {

timespec GetTimespec(const int milliseconds_from_now) {
    timespec ts;

    // Get the current time.
#if USE_CLOCK_GETTIME
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    timeval tv;
    gettimeofday(&tv, nullptr);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#endif

    // Add the specified number of milliseconds to it.
    ts.tv_sec += (milliseconds_from_now / 1000);
    ts.tv_nsec += (milliseconds_from_now % 1000) * 1000000;

    // Normalize.
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    return ts;
}

}  // namespace

Event::Event(bool manual_reset, bool initially_signaled)
    : is_manual_reset_(manual_reset), event_status_(initially_signaled) {
    assert(pthread_mutex_init(&event_mutex_, nullptr) == 0);
    pthread_condattr_t cond_attr;
    assert(pthread_condattr_init(&cond_attr) == 0);
    #if USE_CLOCK_GETTIME && !USE_PTHREAD_COND_TIMEDWAIT_MONOTONIC_NP
    RTC_CHECK(pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC) == 0);
    #endif
    assert(pthread_cond_init(&event_cond_, &cond_attr) == 0);
    pthread_condattr_destroy(&cond_attr);
}

Event::~Event() {
    pthread_mutex_destroy(&event_mutex_);
    pthread_cond_destroy(&event_cond_);
}

void Event::Set() {
    pthread_mutex_lock(&event_mutex_);
    event_status_ = true;
    pthread_cond_broadcast(&event_cond_);
    pthread_mutex_unlock(&event_mutex_);
}

void Event::Reset() {
    pthread_mutex_lock(&event_mutex_);
    event_status_ = false;
    pthread_mutex_unlock(&event_mutex_);
}

bool Event::Wait(const int give_up_after_ms, const int warn_after_ms) {
    // Instant when we'll log a warning message (because we've been waiting so
    // long it might be a bug), but not yet give up waiting. nullopt if we
    // shouldn't log a warning.
    const std::optional<timespec> warn_ts =
        warn_after_ms == kForever ||
                (give_up_after_ms != kForever && warn_after_ms > give_up_after_ms)
            ? std::nullopt
            : std::make_optional(GetTimespec(warn_after_ms));

    // Instant when we'll stop waiting and return an error. nullopt if we should
    // never give up.
    const std::optional<timespec> give_up_ts =
        give_up_after_ms == kForever
            ? std::nullopt
            : std::make_optional(GetTimespec(give_up_after_ms));

    pthread_mutex_lock(&event_mutex_);

    // Wait for `event_cond_` to trigger and `event_status_` to be set, with the
    // given timeout (or without a timeout if none is given).
    const auto wait = [&](const std::optional<timespec> timeout_ts) {
        int error = 0;
        while (!event_status_ && error == 0) {
            if (timeout_ts == std::nullopt) {
                error = pthread_cond_wait(&event_cond_, &event_mutex_);
            } else {
        #if USE_PTHREAD_COND_TIMEDWAIT_MONOTONIC_NP
                error = pthread_cond_timedwait_monotonic_np(&event_cond_, &event_mutex_,
                                                            &*timeout_ts);
        #else
                error =
                    pthread_cond_timedwait(&event_cond_, &event_mutex_, &*timeout_ts);
        #endif
            }
        }
        return error;
    };

    int error;
    if (warn_ts == std::nullopt) {
        error = wait(give_up_ts);
    } else {
        error = wait(warn_ts);
        if (error == ETIMEDOUT) {
            PLOG_WARNING << "Probable deadlock.";
            error = wait(give_up_ts);
        }
    }

    // NOTE(liulk): Exactly one thread will auto-reset this event. All
    // the other threads will think it's unsignaled.  This seems to be
    // consistent with auto-reset events in WEBRTC_WIN
    if (error == 0 && !is_manual_reset_)
        event_status_ = false;

    pthread_mutex_unlock(&event_mutex_);

    return (error == 0);
}

} // namespace naivertc

#endif
