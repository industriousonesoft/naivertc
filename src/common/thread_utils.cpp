#include "common/thread_utils.hpp"
#if defined(NAIVERTC_LINUX)
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#if defined(NAIVERTC_WIN)
#include "common/array_size.hpp"

// The SetThreadDescription API was brought in version 1607 of Windows 10.
// For compatibility with various versions of winuser and avoid clashing with
// a potentially defined type, we use the RTC_ prefix.
typedef HRESULT(WINAPI* RTC_SetThreadDescription)(HANDLE hThread,
                                                  PCWSTR lpThreadDescription);
#endif

namespace naivertc {

PlatformThreadId CurrentThreadId() {
#if defined(NAIVERTC_WIN)
    return GetCurrentThreadId();
#elif defined(NAIVERTC_POSIX)
#if defined(NAIVERTC_MAC) || defined(NAIVERTC_IOS)
    return pthread_mach_thread_np(pthread_self());
#elif defined(NAIVERTC_ANDROID)
    return gettid();
#elif defined(NAIVERTC_FUCHSIA)
    return zx_thread_self();
#elif defined(NAIVERTC_LINUX)
    return syscall(__NR_gettid);
#elif defined(__EMSCRIPTEN__)
    return static_cast<PlatformThreadId>(pthread_self());
#else
    // Default implementation for nacl and solaris.
    return reinterpret_cast<PlatformThreadId>(pthread_self());
#endif
#endif  // defined(NAIVERTC_POSIX)
}

PlatformThreadRef CurrentThreadRef() {
#if defined(NAIVERTC_WIN)
    return GetCurrentThreadId();
#elif defined(NAIVERTC_FUCHSIA)
    return zx_thread_self();
#elif defined(NAIVERTC_POSIX)
    return pthread_self();
#endif
}

bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b) {
#if defined(NAIVERTC_WIN) || defined(NAIVERTC_FUCHSIA)
    return a == b;
#elif defined(NAIVERTC_POSIX)
    return pthread_equal(a, b);
#endif
}

void SetCurrentThreadName(const char* name) {
#if defined(NAIVERTC_WIN)
    // The SetThreadDescription API works even if no debugger is attached.
    // The names set with this API also show up in ETW traces. Very handy.
    static auto set_thread_description_func =
        reinterpret_cast<RTC_SetThreadDescription>(::GetProcAddress(
            ::GetModuleHandleA("Kernel32.dll"), "SetThreadDescription"));
    if (set_thread_description_func) {
        // Convert from ASCII to UTF-16.
        wchar_t wide_thread_name[64];
        for (size_t i = 0; i < arraySize(wide_thread_name) - 1; ++i) {
        wide_thread_name[i] = name[i];
        if (wide_thread_name[i] == L'\0')
            break;
        }
        // Guarantee null-termination.
        wide_thread_name[arraySize(wide_thread_name) - 1] = L'\0';
        set_thread_description_func(::GetCurrentThread(), wide_thread_name);
    }

    // For details see:
    // https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
#pragma pack(push, 8)
    struct {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    } threadname_info = {0x1000, name, static_cast<DWORD>(-1), 0};
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable : 6320 6322)
    __try {
        ::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(ULONG_PTR),
                        reinterpret_cast<ULONG_PTR*>(&threadname_info));
    } __except (EXCEPTION_EXECUTE_HANDLER) {  // NOLINT
    }
#pragma warning(pop)
#elif defined(NAIVERTC_LINUX) || defined(NAIVERTC_ANDROID)
    prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name));  // NOLINT
#elif defined(NAIVERTC_MAC) || defined(NAIVERTC_IOS)
    pthread_setname_np(name);
#endif
}

} // namespace naivertc