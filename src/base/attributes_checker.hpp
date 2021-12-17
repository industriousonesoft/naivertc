#ifndef _BASE_ATTRIBUTES_CHECKER_H_
#define _BASE_ATTRIBUTES_CHECKER_H_

// RTC_HAS_CPP_ATTRIBUTE
//
// A function-like feature checking macro that accepts C++11 style attributes.
// It's a wrapper around `__has_cpp_attribute`, defined by ISO C++ SD-6
// (https://en.cppreference.com/w/cpp/experimental/feature_test). If we don't
// find `__has_cpp_attribute`, will evaluate to 0.
#if defined(__cplusplus) && defined(__has_cpp_attribute)
// NOTE: requiring __cplusplus above should not be necessary, but
// works around https://bugs.llvm.org/show_bug.cgi?id=23435.
#define RTC_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define RTC_HAS_CPP_ATTRIBUTE(x) 0
#endif

// RTC_CONST_INIT
//
// A variable declaration annotated with the `RTC_CONST_INIT` attribute will
// not compile (on supported platforms) unless the variable has a constant
// initializer. This is useful for variables with static and thread storage
// duration, because it guarantees that they will not suffer from the so-called
// "static init order fiasco". Prefer to put this attribute on the most visible
// declaration of the variable, if there's more than one, because code that
// accesses the variable can then use the attribute for optimization.
// NOTE: that this attribute is redundant if the variable is declared constexpr.
#if RTC_HAS_CPP_ATTRIBUTE(clang::require_constant_initialization)
#define RTC_CONST_INIT [[clang::require_constant_initialization]]
#else
#define RTC_CONST_INIT
#endif // RTC_HAS_CPP_ATTRIBUTE(clang::require_constant_initialization)

// RTC_DEPRECATED()
//
// Marks a deprecated class, struct, enum, function, method and variable
// declarations. The macro argument is used as a custom diagnostic message (e.g.
// suggestion of a better alternative).
// Examples:
//
// class RTC_DEPRECATED("Use Bar instead") Foo {...};
//
// RTC_DEPRECATED("Use Baz() instead") void Bar() {...}
//
// template <typename T>
// RTC_DEPRECATED("Use DoThat() instead")
// void DoThis();
//
// NOTE: Every usage of a deprecated entity will trigger a warning when compiled with
// clang's `-Wdeprecated-declarations` option. This option is turned off by
// default, but the warnings will be reported by clang-tidy.
#if defined(__clang__) && defined(__cplusplus) && __cplusplus >= 201103L
#define RTC_DEPRECATED(message) __attribute__((deprecated(message)))
#endif
#ifndef RTC_DEPRECATED
#define RTC_DEPRECATED(message)
#endif

// RTC_MUST_USE_RESULT
//
// Tells the compiler to warn about unused results.
//
// When annotating a function, it must appear as the first part of the
// declaration or definition. The compiler will warn if the return value
// from such a function is unused.
//
// NOTE: `warn_unused_result` is used only for clang but not for gcc.
#if RTC_HAS_CPP_ATTRIBUTE(nodiscard)
#define RTC_MUST_USE_RESULT [[nodiscard]]
#elif defined(__clang__) && RTC_HAS_CPP_ATTRIBUTE(warn_unused_result)
#define RTC_MUST_USE_RESULT __attribute__(warn_unused_result)
#else
#define RTC_MUST_USE_RESULT
#endif

#endif // _BASE_ATTRIBUTES_CHECKER_H_