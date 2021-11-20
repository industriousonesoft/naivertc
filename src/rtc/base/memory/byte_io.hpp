#ifndef _RTC_BASE_MEMORY_BYTE_IO_H_
#define _RTC_BASE_MEMORY_BYTE_IO_H_

#include <stdint.h>
#include <limits>

namespace naivertc {

// According to ISO C strandard ISO/IEC 9899, section 6.2.6.2 (2), the three
// representations of signed integers allowed are two's complement, one's complement
// and sign/magnitude. We can detect which is used by looking at the two last bit of -1,
// which will be 11 in two's complement, 10 in one's complement and 01 in sign/magnitude.

// Assume the if any one signed integer type is two's complement, then will other will be too.
static_assert((-1 & 0x03) == 0x03, "Only two's complement representation of signed integers supported.");

// Plain const char* won't work for static_assert, use #define instead.
#define kSizeErrorMsg "Byte size must be less than or equal to data type size."

// Utility class for getting the unsigned equivalent of a signed type/
template <typename T> 
struct UnsignedOf;

// Below follows specializations of UnsignedOf utility class
template <>
struct UnsignedOf<int8_t> {
    typedef uint8_t Type;
};

template <>
struct UnsignedOf<int16_t> {
    typedef uint16_t Type;
};

template <>
struct UnsignedOf<int32_t> {
    typedef uint32_t Type;
};

template <>
struct UnsignedOf<int64_t> {
    typedef uint64_t Type;
};
    
} // namespace naivertc


#endif