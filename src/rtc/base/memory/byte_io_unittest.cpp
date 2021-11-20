#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {

class T(ByteIOTest) : public testing::Test {
protected:
    T(ByteIOTest)() = default;
    ~T(ByteIOTest)() override = default;

    enum { kAlignments = sizeof(uint64_t) - 1 };

    // Method to create a test value that is not the same when byte reversed.
    template <typename T>
    T CreateTestValue(bool negative, uint8_t num_bytes) {
        // Examples of output:
        // T = int32_t, negative = false, num_bytes = 4: 0x00010203
        // T = int32_t, negative = true, num_bytes = 4: 0xFFFEFDFC
        // T = int32_t, negative = false, num_bytes = 3: 0x000102
        // * T = int32_t, negative = true, num_bytes = 3: 0xFFFEFD

        T val = 0;
        for (uint8_t i = 0; i != num_bytes; ++i) {
            val = (val << 8) + (negative ? (0xFF - i) : (i + 1));
        }

        // This loop will create a sign extend mask if num_bytes if necessary.
        // For the last example (marked * above), the number needs to be sign
        // extended to be a valid int32_t. The sign extend mask is 0xFF000000.
        // Comments for each step with this example below.
        if (std::numeric_limits<T>::is_signed && negative &&
            num_bytes < sizeof(T)) {
            // Start with mask = 0xFFFFFFFF.
            T mask = static_cast<T>(-1);
            // Create a temporary for the lowest byte (0x000000FF).
            const T neg_byte = static_cast<T>(0xFF);
            for (int i = 0; i < num_bytes; ++i) {
                // And the inverse of the temporary and the mask:
                // 0xFFFFFFFF & 0xFFFFFF00 = 0xFFFFFF00.
                // 0xFFFFFF00 & 0xFFFF00FF = 0xFFFF0000.
                // 0xFFFF0000 & 0xFF00FFFF = 0xFF000000.
                mask &= ~(neg_byte << (i * 8));
            }
            // Add the sign extension mask to the actual value.
            val |= mask;
        }
        return val;
    }

    // Populate byte buffer with value, in big endian format.
    template <typename T>
    void PopulateTestData(uint8_t* data, T value, int num_bytes, bool big_endian) {
        if (big_endian) {
            for (int i = 0; i < num_bytes; ++i) {
                data[i] = (value >> ((num_bytes - i - 1) * 8)) & 0xFF;
            }
        } else {
            for (int i = 0; i < num_bytes; ++i) {
                data[i] = (value >> (i * 8)) & 0xFF;
            }
        }
    }

    // Test reading big endian numbers.
    // Template arguments: Type T, read method RM(buffer), B bytes of data.
    template <typename U, U (*RM)(const uint8_t*), int B>
    void TestRead(bool big_endian) {
        // Test both for values that are positive and negative (if signed)
        for (int neg = 0; neg < 2; ++neg) {
            bool negative = neg > 0;

            // Write test value to byte buffer, in big endian format.
            U test_value = CreateTestValue<U>(negative, B);
            uint8_t bytes[B + kAlignments];

            // Make one test for each alignment.
            for (int i = 0; i < kAlignments; ++i) {
                PopulateTestData(bytes + i, test_value, B, big_endian);

                // Check that test value is retrieved from buffer when used read method.
                EXPECT_EQ(test_value, RM(bytes + i));
            }
        }
    }

    // Test writing big endian numbers.
    // Template arguments: Type T, write method WM(buffer, value), B bytes of data
    template <typename U, void (*WM)(uint8_t*, U), int B>
    void TestWrite(bool big_endian) {
        // Test both for values that are positive and negative (if signed).
        for (int neg = 0; neg < 2; ++neg) {
            bool negative = neg > 0;

            // Write test value to byte buffer, in big endian format.
            U test_value = CreateTestValue<U>(negative, B);
            uint8_t expected_bytes[B + kAlignments];
            uint8_t bytes[B + kAlignments];

            // Make one test for each alignment.
            for (int i = 0; i < kAlignments; ++i) {
                PopulateTestData(expected_bytes + i, test_value, B, big_endian);

                // Zero initialize buffer and let WM populate it.
                memset(bytes, 0, B + kAlignments);
                WM(bytes + i, test_value);

                // Check that data produced by WM is big endian as expected.
                for (int j = 0; j < B; ++j) {
                    EXPECT_EQ(expected_bytes[i + j], bytes[i + j]);
                }
            }
        }
    }
};

// Unsigned and BigEndian
MY_TEST_F(ByteIOTest, Test8UBitBigEndian) {
    TestRead<uint8_t, ByteReader<uint8_t>::ReadBigEndian, sizeof(uint8_t)>(true);
    TestWrite<uint8_t, ByteWriter<uint8_t>::WriteBigEndian, sizeof(uint8_t)>(true);
}

MY_TEST_F(ByteIOTest, Test16UBitBigEndian) {
    TestRead<uint16_t, ByteReader<uint16_t>::ReadBigEndian, sizeof(uint16_t)>(true);
    TestWrite<uint16_t, ByteWriter<uint16_t>::WriteBigEndian, sizeof(uint16_t)>(true);
}

MY_TEST_F(ByteIOTest, Test24UBitBigEndian) {
    TestRead<uint32_t, ByteReader<uint32_t, 3>::ReadBigEndian, 3>(true);
    TestWrite<uint32_t, ByteWriter<uint32_t, 3>::WriteBigEndian, 3>(true);
}

MY_TEST_F(ByteIOTest, Test32UBitBigEndian) {
    TestRead<uint32_t, ByteReader<uint32_t>::ReadBigEndian, sizeof(uint32_t)>(true);
    TestWrite<uint32_t, ByteWriter<uint32_t>::WriteBigEndian, sizeof(uint32_t)>(true);
}

MY_TEST_F(ByteIOTest, Test64UBitBigEndian) {
    TestRead<uint64_t, ByteReader<uint64_t>::ReadBigEndian, sizeof(uint64_t)>(true);
    TestWrite<uint64_t, ByteWriter<uint64_t>::WriteBigEndian, sizeof(uint64_t)>(true);
}

// Unsigned and LittleEndian
MY_TEST_F(ByteIOTest, Test8UBitLittleEndian) {
    TestRead<uint8_t, ByteReader<uint8_t>::ReadLittleEndian, sizeof(uint8_t)>(false);
    TestWrite<uint8_t, ByteWriter<uint8_t>::WriteLittleEndian, sizeof(uint8_t)>(false);
}

MY_TEST_F(ByteIOTest, Test16UBitLittleEndian) {
    TestRead<uint16_t, ByteReader<uint16_t>::ReadLittleEndian, sizeof(uint16_t)>(false);
    TestWrite<uint16_t, ByteWriter<uint16_t>::WriteLittleEndian, sizeof(uint16_t)>(false);
}

MY_TEST_F(ByteIOTest, Test24UBitLittleEndian) {
    TestRead<uint32_t, ByteReader<uint32_t, 3>::ReadLittleEndian, 3>(false);
    TestWrite<uint32_t, ByteWriter<uint32_t, 3>::WriteLittleEndian, 3>(false);
}

MY_TEST_F(ByteIOTest, Test32UBitLittleEndian) {
    TestRead<uint32_t, ByteReader<uint32_t>::ReadLittleEndian, sizeof(uint32_t)>(false);
    TestWrite<uint32_t, ByteWriter<uint32_t>::WriteLittleEndian, sizeof(uint32_t)>(false);
}

MY_TEST_F(ByteIOTest, Test64UBitLittleEndian) {
    TestRead<uint64_t, ByteReader<uint64_t>::ReadLittleEndian, sizeof(uint64_t)>(false);
    TestWrite<uint64_t, ByteWriter<uint64_t>::WriteLittleEndian, sizeof(uint64_t)>(false);
}

// Signed and BigEndian
MY_TEST_F(ByteIOTest, Test8SBitBigEndian) {
    TestRead<int8_t, ByteReader<int8_t>::ReadBigEndian, sizeof(int8_t)>(true);
    TestWrite<int8_t, ByteWriter<int8_t>::WriteBigEndian, sizeof(int8_t)>(true);
}

MY_TEST_F(ByteIOTest, Test16SBitBigEndian) {
    TestRead<int16_t, ByteReader<int16_t>::ReadBigEndian, sizeof(int16_t)>(true);
    TestWrite<int16_t, ByteWriter<int16_t>::WriteBigEndian, sizeof(int16_t)>(true);
}

MY_TEST_F(ByteIOTest, Test24SBitBigEndian) {
    TestRead<int32_t, ByteReader<int32_t, 3>::ReadBigEndian, 3>(true);
    TestWrite<int32_t, ByteWriter<int32_t, 3>::WriteBigEndian, 3>(true);
}

MY_TEST_F(ByteIOTest, Test32SBitBigEndian) {
    TestRead<int32_t, ByteReader<int32_t>::ReadBigEndian, sizeof(int32_t)>(true);
    TestWrite<int32_t, ByteWriter<int32_t>::WriteBigEndian, sizeof(int32_t)>(true);
}

MY_TEST_F(ByteIOTest, Test64SBitBigEndian) {
    TestRead<int64_t, ByteReader<int64_t>::ReadBigEndian, sizeof(int64_t)>(true);
    TestWrite<int64_t, ByteWriter<int64_t>::WriteBigEndian, sizeof(int64_t)>(true);
}

// Signed and LittleEndian
MY_TEST_F(ByteIOTest, Test8SBitLittleEndian) {
    TestRead<int8_t, ByteReader<int8_t>::ReadLittleEndian, sizeof(int8_t)>(false);
    TestWrite<int8_t, ByteWriter<int8_t>::WriteLittleEndian, sizeof(int8_t)>(false);
}

MY_TEST_F(ByteIOTest, Test16SBitLittleEndian) {
    TestRead<int16_t, ByteReader<int16_t>::ReadLittleEndian, sizeof(int16_t)>(false);
    TestWrite<int16_t, ByteWriter<int16_t>::WriteLittleEndian, sizeof(int16_t)>(false);
}

MY_TEST_F(ByteIOTest, Test24SBitLittleEndian) {
    TestRead<int32_t, ByteReader<int32_t, 3>::ReadLittleEndian, 3>(false);
    TestWrite<int32_t, ByteWriter<int32_t, 3>::WriteLittleEndian, 3>(false);
}

MY_TEST_F(ByteIOTest, Test32SBitLittleEndian) {
    TestRead<int32_t, ByteReader<int32_t>::ReadLittleEndian, sizeof(int32_t)>(false);
    TestWrite<int32_t, ByteWriter<int32_t>::WriteLittleEndian, sizeof(int32_t)>(false);
}

MY_TEST_F(ByteIOTest, Test64SBitLittleEndian) {
    TestRead<int64_t, ByteReader<int64_t>::ReadLittleEndian, sizeof(int64_t)>(false);
    TestWrite<int64_t, ByteWriter<int64_t>::WriteLittleEndian, sizeof(int64_t)>(false);
}

MY_TEST_F(ByteIOTest, TestFixedByteArrayUnsignedReadBigEndian) {
    uint8_t data[8] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88};
    uint64_t value = ByteReader<uint64_t, 2>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEE), value);
    value = ByteReader<uint64_t, 3>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEEDD), value);
    value = ByteReader<uint64_t, 4>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEEDDCC), value);
    value = ByteReader<uint64_t, 5>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEEDDCCBB), value);
    value = ByteReader<uint64_t, 6>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEEDDCCBBAA), value);
    value = ByteReader<uint64_t, 7>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEEDDCCBBAA99), value);
    value = ByteReader<uint64_t, 8>::ReadBigEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xFFEEDDCCBBAA9988), value);
}

MY_TEST_F(ByteIOTest, TestFixedByteArrayUnsignedReadLittleEndian) {
    uint8_t data[8] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88};
    uint64_t value = ByteReader<uint64_t, 2>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xEEFF), value);
    value = ByteReader<uint64_t, 3>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xDDEEFF), value);
    value = ByteReader<uint64_t, 4>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xCCDDEEFF), value);
    value = ByteReader<uint64_t, 5>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xBBCCDDEEFF), value);
    value = ByteReader<uint64_t, 6>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0xAABBCCDDEEFF), value);
    value = ByteReader<uint64_t, 7>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0x99AABBCCDDEEFF), value);
    value = ByteReader<uint64_t, 8>::ReadLittleEndian(data);
    EXPECT_EQ(static_cast<uint64_t>(0x8899AABBCCDDEEFF), value);
}
    
} // namespace test
} // namespace naivert 
