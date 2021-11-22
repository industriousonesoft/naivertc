#ifndef _COMMON_THREAD_UTILS_H_
#define _COMMON_THREAD_UTILS_H_

// clang-format off
// clang formating would change include order.
#if defined(NAIVERTC_WIN)
// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h. To include win32.h directly, it must be broken out into its own
// build target.
#include <winsock2.h>
#include <windows.h>
#elif defined(NAIVERTC_FUCHSIA)
#include <zircon/types.h>
#include <zircon/process.h>
#elif defined(NAIVERTC_POSIX)
#include <pthread.h>
#include <unistd.h>
#if defined(NAIVERTC_MAC)
#include <pthread_spis.h>
#endif // defined(NAIVERTC_MAC)
#endif // defined(NAIVERTC_WIN)
// clang-format on

namespace naivertc {

#if defined(NAIVERTC_WIN)
typedef DWORD PlatformThreadId;
typedef DWORD PlatformThreadRef;
#elif defined(NAIVERTC_FUCHSIA)
typedef zx_handle_t PlatformThreadId;
typedef zx_handle_t PlatformThreadRef;
#elif defined(NAIVERTC_POSIX)
typedef pid_t PlatformThreadId;
typedef pthread_t PlatformThreadRef;
#endif

// Retrieve the ID of the current thread.
PlatformThreadId CurrentThreadId();

// Retrieves a reference to the current thread. On Windows, this is the same
// as CurrentThreadId. On other platforms it's the pthread_t returned by
// pthread_self().
PlatformThreadRef CurrentThreadRef();

// Compares two thread identifiers for equality.
bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b);

// Sets the current thread name.
void SetCurrentThreadName(const char* name);

} // namespace naivertc

#endif