#include "common/event.hpp"

#if defined(NAIVERTC_WIN)

#include <windows.h>

namespace naivertc {

Event::Event(bool manual_reset, bool initially_signaled) {
    event_handle_ = ::CreateEvent(nullptr,  // Security attributes.
                                    manual_reset, initially_signaled,
                                    nullptr);  // Name.
    RTC_CHECK(event_handle_);
}

Event::~Event() {
    CloseHandle(event_handle_);
}

void Event::Set() {
    SetEvent(event_handle_);
}

void Event::Reset() {
    ResetEvent(event_handle_);
}

bool Event::Wait(const int give_up_after_ms, int /*warn_after_ms*/) {
    ScopedYieldPolicy::YieldExecution();
    const DWORD ms = give_up_after_ms == kForever ? INFINITE : give_up_after_ms;
    return (WaitForSingleObject(event_handle_, ms) == WAIT_OBJECT_0);
}

} // namespace naivertc

#endif