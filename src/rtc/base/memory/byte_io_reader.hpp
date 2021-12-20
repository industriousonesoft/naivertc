#ifndef _RTC_BASE_MEMORY_BYTE_IO_READER_H_
#define _RTC_BASE_MEMORY_BYTE_IO_READER_H_

#include "base/defines.hpp"
#include "rtc/base/memory/byte_io.hpp"

namespace naivertc {

// Class for reading integers from a sequence of bytes.
// T = type of integer, B = bytes to read, is_signed = true is signed integer.
// If is_signed is true and B < sizeof(T), sign extension might be needed.
template <
    typename T, 
    unsigned int B = sizeof(T),
    bool is_signed = std::numeric_limits<T>::is_signed>
class ByteReader;

// Specialization of ByteReader for unsigned types.
template <typename T, unsigned int B>
class RTC_CPP_EXPORT ByteReader<T, B, false> {
public:
    static T ReadBigEndian(const uint8_t* data) {
        static_assert(B <= sizeof(T), kSizeErrorMsg);
        return InternalReadBigEndian(data);
    }

    static T ReadLittleEndian(const uint8_t* data) {
        static_assert(B < sizeof(T), kSizeErrorMsg);
        return InternalReadLittleEndian(data);
    }

private:
    static T InternalReadBigEndian(const uint8_t* data) {
        T val(0);
        for (unsigned int i = 0; i < B; ++i) {
            val |= static_cast<T>(data[i]) << ((B - 1 - i) * 8);
        }
        return val;
    }

    static T InternalReadLittleEndian(const uint8_t* data) {
        T val(0);
        for (unsigned int i = 0; i < B; ++i) {
            val |=  static_cast<T>(data[i]) << (i * 8);
        }
        return val;
    }
};

// Specialization of ByteReader for signed types
template <typename T, unsigned int B> 
class RTC_CPP_EXPORT ByteReader<T, B, true> {
public:
    typedef typename UnsignedOf<T>::Type U;

    static T ReadBigEndian(const uint8_t* data) {
        U unsigned_val = ByteReader<T, B, false>::ReadBigEndian(data);
        if (B < sizeof(T)) {
            unsigned_val = SignExtend(unsigned_val);
        }
        return ReinterpretAsSigned(unsigned_val);
    }

    static T ReadLittleEndian(const uint8_t* data) {
        U unsigned_val = ByteReader<T, B, false>::ReadLittleEndian(data);
        if (B < sizeof(T)) {
            unsigned_val = SignExtend(unsigned_val);
        }
        return ReinterpretAsSigned(unsigned_val);
    }

private:
    // As a hack to avoid implementation-specific or undefined behavior when 
    // bit-shifting or casting signed integers, read as a signed equivalent
    // instead and convert to signed. This is safe since we have asserted that 
    // two's complement for is used.
    static T ReinterpretAsSigned(U unsigned_val) {
        // An unsigned value with only the highest order bit set (ex 0x80).
        const U kUnsignedHighestBitMask = static_cast<U>(1) << ((sizeof(U) * 8) - 1);

        // A signed value with only the highest bit set. Since this is two's complement form,
        // We can use the min value from std::numeric_limits
        const T kSignedHighestBitMask = std::numeric_limits<T>::min();

        T val;
        if ((unsigned_val & kUnsignedHighestBitMask) != 0) {
            // Casting is only safe when unsigned value can be represented in the signed target
            // type, so mask out highest bit and mask it back manually.
            val = static_cast<T>(unsigned_val & ~kUnsignedHighestBitMask);
            val |= kSignedHighestBitMask;
        } else {
            val = static_cast<T>(unsigned_val);
        }
        return val;
    }

    // If number of bytes is less than native data type (eg 24 bits in int32_t),
    // and the most significant bit of the actual data is set, we must sign extend
    // the remaining byte(s) with ones so that the correct negative number is retained.
    // Ex: 0x8203EF -> 0xFF8203EF, but 0x7203EF -> 0x007203EF
    static U SignExtend(const U val) {
        const uint8_t kMaskBit = static_cast<uint8_t>(val >> ((B - 1) * 8));
        if ((kMaskBit & 0x80) != 0) {
            // Create a mask where all bits used by the B bytes are set to one,
            // for instance 0x00FFFFFF for B = 3, Bit-wise invert that mask (0xFF000000 in the example above)
            // and add it to the input value. The "B % sizeof(T)" is a workaround to undefined values warnings
            // for B == sizeof(T), in which case this code won't be called anyway.
            const U kUsedBitMask = (1 << ((B % sizeof(T)) * 8)) - 1;
            return ~kUsedBitMask | val;
        }
        return val;
    }
};

// Specializations for single bytes
template <typename T>
class RTC_CPP_EXPORT ByteReader<T, 1, false> {
public:
    static T ReadBigEndian(const uint8_t* data) {
        static_assert(sizeof(T) == 1, kSizeErrorMsg);
        return data[0];
    }

    static T ReadLittleEndian(const uint8_t* data) {
        static_assert(sizeof(T) == 1, kSizeErrorMsg);
        return data[0];
    }
};

// Specializations for two bytes
template <typename T>
class RTC_CPP_EXPORT ByteReader<T, 2, false> {
public:
    static T ReadBigEndian(const uint8_t* data) {
        static_assert(sizeof(T) >= 2, kSizeErrorMsg);
        return (data[0] << 8) | data[1];
    }

    static T ReadLittleEndian(const uint8_t* data) {
        static_assert(sizeof(T) >= 2, kSizeErrorMsg);
        return data[0] | (data[1] << 8);
    }
};

// Specializations for four bytes
template <typename T>
class RTC_CPP_EXPORT ByteReader<T, 4, false> {
public:
    static T ReadBigEndian(const uint8_t* data) {
        static_assert(sizeof(T) >= 4, kSizeErrorMsg);
        return Get(data, 0) << 24 | Get(data, 1) << 16 | Get(data, 2) << 8 | Get(data, 3);
    }

    static T ReadLittleEndian(const uint8_t* data) {
        static_assert(sizeof(T) >= 4, kSizeErrorMsg);
        return Get(data, 0) | Get(data, 1) << 8 | Get(data, 2) << 16 | Get(data, 3) << 24;
    }
private:
    inline static T Get(const uint8_t* data, unsigned int index) {
        return static_cast<T>(data[index]);
    }
};

// Specializations for eight bytes
template <typename T>
class RTC_CPP_EXPORT ByteReader<T, 8, false> {
public:
    static T ReadBigEndian(const uint8_t* data) {
        static_assert(sizeof(T) >= 8, kSizeErrorMsg);
        return Get(data, 0) << 56 | Get(data, 1) << 48 | Get(data, 2) << 40 | Get(data, 3) << 32 |
                Get(data, 4) << 24 | Get(data, 5) << 16 | Get(data, 6) << 8 | Get(data, 7);
    }

    static T ReadLittleEndian(const uint8_t* data) {
        static_assert(sizeof(T) >= 8, kSizeErrorMsg);
        return Get(data, 0) | Get(data, 1) << 8 | Get(data, 2) << 16 | Get(data, 3) << 24 |
                Get(data, 4) << 32 | Get(data, 5) << 40 | Get(data, 6) << 48 | Get(data, 7) << 56;
    }
private:
    inline static T Get(const uint8_t* data, unsigned int index) {
        return static_cast<T>(data[index]);
    }
};

    
} // namespace naivertc


#endif