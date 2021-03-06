#include "rtc/base/copy_on_write_buffer.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

// clang-format off
constexpr uint8_t kTestData[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                             0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};

constexpr uint8_t kTestData2[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

// clang-format on

void EnsureBuffersShareData(const CopyOnWriteBuffer& buf1,
                            const CopyOnWriteBuffer& buf2) {
    // Data is shared between buffers.
    EXPECT_EQ(buf1.size(), buf2.size());
    EXPECT_EQ(buf1.capacity(), buf2.capacity());
    EXPECT_EQ(buf1.cdata(), buf2.cdata());
    EXPECT_EQ(buf1, buf2);
}

void EnsureBuffersDontShareData(const CopyOnWriteBuffer& buf1,
                                const CopyOnWriteBuffer& buf2) {
    // Data is not shared between buffers.
    EXPECT_NE(buf1.cdata(), buf2.cdata());
}
} // namespace 

MY_TEST(CopyOnWriteBufferTest, TestCreateEmptyData) {
    CopyOnWriteBuffer buf(static_cast<const uint8_t*>(nullptr), size_t(0));
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_EQ(buf.capacity(), 0u);
    EXPECT_EQ(buf.cdata(), nullptr);
}

MY_TEST(CopyOnWriteBufferTest, TestMoveConstruct) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    size_t buf1_size = buf1.size();
    size_t buf1_capacity = buf1.capacity();
    const uint8_t* buf1_data = buf1.cdata();

    CopyOnWriteBuffer buf2(std::move(buf1));
    EXPECT_EQ(buf1.size(), 0u);
    EXPECT_EQ(buf1.capacity(), 0u);
    EXPECT_EQ(buf1.cdata(), nullptr);
    EXPECT_EQ(buf2.size(), buf1_size);
    EXPECT_EQ(buf2.capacity(), buf1_capacity);
    EXPECT_EQ(buf2.cdata(), buf1_data);
}

MY_TEST(CopyOnWriteBufferTest, TestMoveAssign) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    size_t buf1_size = buf1.size();
    size_t buf1_capacity = buf1.capacity();
    const uint8_t* buf1_data = buf1.cdata();

    CopyOnWriteBuffer buf2;
    buf2 = std::move(buf1);
    EXPECT_EQ(buf1.size(), 0u);
    EXPECT_EQ(buf1.capacity(), 0u);
    EXPECT_EQ(buf1.cdata(), nullptr);
    EXPECT_EQ(buf2.size(), buf1_size);
    EXPECT_EQ(buf2.capacity(), buf1_capacity);
    EXPECT_EQ(buf2.cdata(), buf1_data);
}

MY_TEST(CopyOnWriteBufferTest, SetEmptyData) {
    CopyOnWriteBuffer buf(10);

    buf.Assign<uint8_t>(nullptr, size_t(0));

    EXPECT_EQ(0u, buf.size());
}

MY_TEST(CopyOnWriteBufferTest, SetDataNoMoreThanCapacityDoesntCauseReallocation) {
    CopyOnWriteBuffer buf1(3, 10);
    const uint8_t* const original_allocation = buf1.cdata();

    buf1.Assign(kTestData, 10);

    EXPECT_EQ(original_allocation, buf1.cdata());
    auto buf2 = CopyOnWriteBuffer(kTestData, 10);
    EXPECT_EQ(buf1.size(), buf2.size());
    EXPECT_EQ(buf1.capacity(), buf2.capacity());
    EXPECT_EQ(0, memcmp(buf1.cdata(), buf2.cdata(), buf1.size()));
    EXPECT_EQ(buf1, buf2);
}

MY_TEST(CopyOnWriteBufferTest, SetSizeCloneContent) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);

    buf2.Resize(16);

    EXPECT_EQ(buf2.size(), 16u);
    EXPECT_EQ(0, memcmp(buf2.cdata(), kTestData, 3));
}

MY_TEST(CopyOnWriteBufferTest, SetSizeMayIncreaseCapacity) {
    CopyOnWriteBuffer buf(kTestData, 3, 10);

    buf.Resize(16);

    EXPECT_EQ(16u, buf.size());
    EXPECT_EQ(16u, buf.capacity());
}

MY_TEST(CopyOnWriteBufferTest, SetSizeDoesntDecreaseCapacity) {
    CopyOnWriteBuffer buf1(kTestData, 5, 10);
    CopyOnWriteBuffer buf2(buf1);

    buf2.Resize(2);

    EXPECT_EQ(2u, buf2.size());
    EXPECT_EQ(10u, buf2.capacity());
}

MY_TEST(CopyOnWriteBufferTest, ClearDoesntChangeOriginal) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    const uint8_t* const original_allocation = buf1.cdata();
    CopyOnWriteBuffer buf2(buf1);

    buf2.Clear();

    EnsureBuffersDontShareData(buf1, buf2);
    EXPECT_EQ(3u, buf1.size());
    EXPECT_EQ(10u, buf1.capacity());
    EXPECT_EQ(original_allocation, buf1.cdata());
    EXPECT_EQ(0u, buf2.size());
}

MY_TEST(CopyOnWriteBufferTest, ClearDoesntChangeCapacity) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);

    buf2.Clear();

    EXPECT_EQ(0u, buf2.size());
    EXPECT_EQ(10u, buf2.capacity());
}

MY_TEST(CopyOnWriteBufferTest, DataAccessorDoesntCloneData) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);
    EXPECT_EQ(buf1.cdata(), buf2.cdata());
}

MY_TEST(CopyOnWriteBufferTest, MutableDataClonesDataWhenShared) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);
    const uint8_t* cdata = buf1.cdata();
    // Do clone
    uint8_t* data1 = buf1.data();
    uint8_t* data2 = buf2.data();
    // buf1 was cloned above.
    EXPECT_NE(data1, cdata);
    // Therefore buf2 was no longer sharing data and was not cloned.
    EXPECT_EQ(data2, cdata);
}

MY_TEST(CopyOnWriteBufferTest, WritingCopiesData) {
    CopyOnWriteBuffer buf1(kTestData, 10, 10);
    CopyOnWriteBuffer buf2(buf1);
    EXPECT_EQ(buf1, buf2);
    buf2[3] = 0xaa;
    EXPECT_NE(buf1.cdata() + 3, buf2.cdata());
    EXPECT_EQ(0, memcmp(buf1.cdata(), kTestData, 10));
    EXPECT_NE(buf1, buf2);
}

MY_TEST(CopyOnWriteBufferTest, AssignDataClonesDataWhenShared) {
    CopyOnWriteBuffer buf1(kTestData, 10, 10);
    CopyOnWriteBuffer buf2(buf1);
    EXPECT_EQ(buf1, buf2);
    buf2.Assign(kTestData2, 5);
    EXPECT_NE(buf1, buf2);
    EXPECT_EQ(buf2.size(), 5);
    EXPECT_EQ(0, memcmp(buf2.cdata(), kTestData2, 5));
}

MY_TEST(CopyOnWriteBufferTest, AppendDataClonesDataWhenShared) {
    CopyOnWriteBuffer buf1(kTestData, 10, 10);
    CopyOnWriteBuffer buf2(buf1);
    EXPECT_EQ(buf1, buf2);
    buf2.Append(kTestData2, 5);
    EXPECT_NE(buf1, buf2);
    EXPECT_EQ(buf2.size(), 15);
    EXPECT_EQ(0, memcmp(buf2.cdata() + 10, kTestData2, 5));
}

MY_TEST(CopyOnWriteBufferTest, InsertDataClonesDataWhenShared) {
    CopyOnWriteBuffer buf1(kTestData, 10, 10);
    CopyOnWriteBuffer buf2(buf1);
    EXPECT_EQ(buf1, buf2);
    buf2.Insert(buf2.begin(), kTestData2, 5);
    EXPECT_NE(buf1, buf2);
    EXPECT_EQ(buf2.size(), 15);
    EXPECT_EQ(0, memcmp(buf2.cdata(), kTestData2, 5));
    EXPECT_EQ(0, memcmp(buf2.cdata() + 5, kTestData, 10));
}

MY_TEST(CopyOnWriteBufferTest, SwapDataClonesDataWhenShared) {
    CopyOnWriteBuffer buf1(kTestData, 10, 10);
    CopyOnWriteBuffer buf2;
    buf1.Swap(buf2);
    EXPECT_EQ(nullptr, buf1.cdata());
    EXPECT_EQ(0, buf1.size());
    EXPECT_EQ(0, memcmp(buf2.cdata(), kTestData, 10));
}

} // namespace test
} // namespace naivertc