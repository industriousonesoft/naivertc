#ifndef _BASE_FEATURES_CHECKER_H_
#define _BASE_FEATURES_CHECKER_H_

// RTC_SUPPORT_TLS
// RTC_SUPPORT_TLS (Thread-local storage) is defined to 1 when `__thread` should be supported.
// We assume `__thread` is supported on Linux when compiled with Clang or compiled
// against libstdc++ with _GLIBCXX_HAVE_TLS defined.
#ifdef RTC_SUPPORT_TLS
#error RTC_SUPPORT_TLS cannot be directly set
#elif defined(__linux__) && (defined(__clang__) || defined(_GLIBCXX_HAVE_TLS))
#define RTC_SUPPORT_TLS 1
#endif

// RTC_SUPPORT_THREAD_LOCAL
// Checks whether C++11's `thread_local` storage duration specifier is
// supported.
#ifdef RTC_SUPPORT_THREAD_LOCAL
#error RTC_SUPPORT_THREAD_LOCAL cannot be directly set
#elif defined(__APPLE__)
// Notes:
// * Xcode's clang did not support `thread_local` until version 8, and
//   even then not for all iOS < 9.0.
// * Xcode 9.3 started disallowing `thread_local` for 32-bit iOS simulator
//   targeting iOS 9.x.
// * Xcode 10 moves the deployment target check for iOS < 9.0 to link time
//   making __has_feature unreliable there.
//
// Otherwise, `__has_feature` is only supported by Clang so it has be inside
// `defined(__APPLE__)` check.
#if __has_feature(cxx_thread_local) && \
    !(TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0)
#define RTC_SUPPORT_THREAD_LOCAL 1
#endif
#else  // !defined(__APPLE__)
#define RTC_SUPPORT_THREAD_LOCAL 1
#endif

// There are platforms for which TLS should not be used even though the compiler
// makes it seem like it's supported (Android NDK < r12b for example).
// This is primarily because of linker problems and toolchain misconfiguration:
// Abseil does not intend to support this indefinitely. Currently, the newest
// toolchain that we intend to support that requires this behavior is the
// r11 NDK - allowing for a 5 year support window on that means this option
// is likely to be removed around June of 2021.
// TLS isn't supported until NDK r12b per
// https://developer.android.com/ndk/downloads/revision_history.html
// Since NDK r16, `__NDK_MAJOR__` and `__NDK_MINOR__` are defined in
// <android/ndk-version.h>. For NDK < r16, users should define these macros,
// e.g. `-D__NDK_MAJOR__=11 -D__NKD_MINOR__=0` for NDK r11.
#if defined(__ANDROID__) && defined(__clang__)
#if __has_include(<android/ndk-version.h>)
#include <android/ndk-version.h>
#endif  // __has_include(<android/ndk-version.h>)
#if defined(__ANDROID__) && defined(__clang__) && defined(__NDK_MAJOR__) && \
    defined(__NDK_MINOR__) &&                                               \
    ((__NDK_MAJOR__ < 12) || ((__NDK_MAJOR__ == 12) && (__NDK_MINOR__ < 1)))
#undef RTC_SUPPORT_TLS
#undef RTC_SUPPORT_THREAD_LOCAL
#endif
#endif  // defined(__ANDROID__) && defined(__clang__)
// Emscripten doesn't yet support `thread_local` or `__thread`.
// https://github.com/emscripten-core/emscripten/issues/3502
#if defined(__EMSCRIPTEN__)
#undef RTC_SUPPORT_TLS
#undef RTC_SUPPORT_THREAD_LOCAL
#endif  // defined(__EMSCRIPTEN__)

#endif // _BASE_FEATURE_CHECKER_THREAD_LOCAL_H_