#ifndef _RTC_BYTE_WRITER_H_
#define _RTC_BYTE_WRITER_H_

#include "base/defines.hpp"
#include "rtc/base/byte_io.hpp"

namespace naivertc {

// Class for writing integers to a sequence of bytes
// T = type of integer, B = bytes to write
template <
    typename T,
    unsigned int B = sizeof(T),
    bool is_signed = std::numeric_limits<T>::is_signed>
class ByteWriter;

// Specialization of ByteWriter for unsigned types
template <typename T, unsigned int B>
class RTC_CPP_EXPORT ByteWriter<T, B, false> {
public:
    static void WriteBigEndian(uint8_t* data, T val) {
        static_assert(B < sizeof(T), kSizeErrorMsg);
        for (unsigned int i = 0; i < B; ++i) {
            data[i] = val >> ((B - 1 - i) * 8);
        }
    }

    static void WriteLittleEndian(uint8_t* data, T val) {
        static_assert(B < sizeof(T), kSizeErrorMsg);
        for (unsigned int i = 0; i < B; ++i) {
            data[i] = val >> (i * 8);
        }
    }
};

// Specialization of ByteWriter for signed types
template <typename T, unsigned int B>
class RTC_CPP_EXPORT ByteWriter<T, B, true> {
public:
    typedef typename UnsignedOf<T>::Type U;

    static void WriteBigEndian(uint8_t* data, T val) {
        ByteWriter<T, B, false>::WriteBigEndian(data, ReinterpretAsUnsigned(val));
    }

    static void WriteLittleEndian(uint8_t* data, T val) {
        ByteWriter<T, B, false>::WriteLittleEndian(data, ReinterpretAsUnsigned(val));
    }
private:
    static U ReinterpretAsUnsigned(T val) {
        // According to ISO C standard ISO/IEC 9899, section 6.3.1.3 (1,2) a conversion
        // from signed to unsigned keeps the value if the new type can represent it, and
        // otherwise adds one more than the max value of T util the value is in range. For
        // two's complement, this fortunately means that the two-wise value will be intact.
        // Thus, since we have asserted that two's complement form is actually used. s simple
        // cast is sufficient.
        return static_cast<U>(val);
    }
};

// Specialization for single byte
template <typename T>
class RTC_CPP_EXPORT ByteWriter<T, 1, false> {
public:
    static void WriteBigEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) == 1, kSizeErrorMsg);
        data[0] = val;
    }
    static void WriteLittleEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) == 1, kSizeErrorMsg);
        data[0] = val;
    }
};

// Specialization for two byte
template <typename T>
class RTC_CPP_EXPORT ByteWriter<T, 2, false> {
public:
    static void WriteBigEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) >= 2, kSizeErrorMsg);
        data[0] = val >> 8;
        data[1] = val;
    }
    static void WriteLittleEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) >= 2, kSizeErrorMsg);
        data[0] = val;
        data[1] = val >> 8;
    }
};

// Specialization for four byte
template <typename T>
class RTC_CPP_EXPORT ByteWriter<T, 4, false> {
public:
    static void WriteBigEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) >= 4, kSizeErrorMsg);
        data[0] = val >> 24;
        data[1] = val >> 16;
        data[2] = val >> 8;
        data[3] = val;
    }
    static void WriteLittleEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) >= 4, kSizeErrorMsg);
        data[0] = val;
        data[1] = val >> 8;
        data[2] = val >> 16;
        data[3] = val >> 24;
    }
};

// Specialization for eight byte
template <typename T>
class RTC_CPP_EXPORT ByteWriter<T, 8, false> {
public:
    static void WriteBigEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) >= 8, kSizeErrorMsg);
        data[0] = val >> 56;
        data[1] = val >> 48;
        data[2] = val >> 40;
        data[3] = val >> 32;
        data[4] = val >> 24;
        data[5] = val >> 16;
        data[6] = val >> 8;
        data[7] = val;
    }
    static void WriteLittleEndian(uint8_t* data, T val) {
        static_assert(sizeof(T) >= 8, kSizeErrorMsg);
        data[0] = val;
        data[1] = val >> 8;
        data[2] = val >> 16;
        data[3] = val >> 24;
        data[4] = val >> 32;
        data[5] = val >> 40;
        data[6] = val >> 48;
        data[7] = val >> 56;
    }
};

    
} // namespace naivert 


#endif