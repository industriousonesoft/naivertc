#ifndef RTC_BASE_ARRAYSIZE_H_
#define RTC_BASE_ARRAYSIZE_H_

#include <stddef.h>

// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N>
constexpr size_t arraySize(T (&)[N]) noexcept {
    return N;
}

#endif