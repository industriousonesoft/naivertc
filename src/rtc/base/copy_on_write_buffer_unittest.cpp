#include "rtc/base/copy_on_write_buffer.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {
namespace {

// clang-format off
const uint8_t kTestData[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                             0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
// clang-format on

void EnsureBuffersShareData(const CopyOnWriteBuffer& buf1,
                            const CopyOnWriteBuffer& buf2) {
    // Data is shared between buffers.
    EXPECT_EQ(buf1.size(), buf2.size());
    EXPECT_EQ(buf1.capacity(), buf2.capacity());
    const uint8_t* data1 = buf1.cdata();
    const uint8_t* data2 = buf2.cdata();
    EXPECT_EQ(data1, data2);
    EXPECT_EQ(buf1, buf2);
}

void EnsureBuffersDontShareData(const CopyOnWriteBuffer& buf1,
                                const CopyOnWriteBuffer& buf2) {
    // Data is not shared between buffers.
    const uint8_t* data1 = buf1.cdata();
    const uint8_t* data2 = buf2.cdata();
    EXPECT_NE(data1, data2);
}
} // namespace 

TEST(CopyOnWriteBufferTest, TestCreateEmptyData) {
    CopyOnWriteBuffer buf(static_cast<const uint8_t*>(nullptr), 0);
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_EQ(buf.capacity(), 0u);
    EXPECT_EQ(buf.data(), nullptr);
}

TEST(CopyOnWriteBufferTest, TestMoveConstruct) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    size_t buf1_size = buf1.size();
    size_t buf1_capacity = buf1.capacity();
    const uint8_t* buf1_data = buf1.cdata();

    CopyOnWriteBuffer buf2(std::move(buf1));
    EXPECT_EQ(buf1.size(), 0u);
    EXPECT_EQ(buf1.capacity(), 0u);
    EXPECT_EQ(buf1.data(), nullptr);
    EXPECT_EQ(buf2.size(), buf1_size);
    EXPECT_EQ(buf2.capacity(), buf1_capacity);
    EXPECT_EQ(buf2.data(), buf1_data);
}

TEST(CopyOnWriteBufferTest, TestMoveAssign) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    size_t buf1_size = buf1.size();
    size_t buf1_capacity = buf1.capacity();
    const uint8_t* buf1_data = buf1.cdata();

    CopyOnWriteBuffer buf2;
    buf2 = std::move(buf1);
    EXPECT_EQ(buf1.size(), 0u);
    EXPECT_EQ(buf1.capacity(), 0u);
    EXPECT_EQ(buf1.data(), nullptr);
    EXPECT_EQ(buf2.size(), buf1_size);
    EXPECT_EQ(buf2.capacity(), buf1_capacity);
    EXPECT_EQ(buf2.data(), buf1_data);
}

TEST(CopyOnWriteBufferTest, SetEmptyData) {
    CopyOnWriteBuffer buf(10);

    buf.Assign(nullptr, 0);

    EXPECT_EQ(0u, buf.size());
}

TEST(CopyOnWriteBufferTest, SetDataNoMoreThanCapacityDoesntCauseReallocation) {
    CopyOnWriteBuffer buf1(3, 10);
    const uint8_t* const original_allocation = buf1.data();

    buf1.Assign(kTestData, 10);

    EXPECT_EQ(original_allocation, buf1.data());
    auto buf2 = CopyOnWriteBuffer(kTestData, 10);
    EXPECT_EQ(buf1.size(), buf2.size());
    EXPECT_EQ(buf1.capacity(), buf2.capacity());
    EXPECT_EQ(0, memcmp(buf1.data(), buf2.data(), buf1.size()));
    EXPECT_EQ(buf1, buf2);
}

TEST(CopyOnWriteBufferTest, SetSizeCloneContent) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);

    buf2.Resize(16);

    EXPECT_EQ(buf2.size(), 16u);
    EXPECT_EQ(0, memcmp(buf2.data(), kTestData, 3));
}

TEST(CopyOnWriteBufferTest, SetSizeMayIncreaseCapacity) {
    CopyOnWriteBuffer buf(kTestData, 3, 10);

    buf.Resize(16);

    EXPECT_EQ(16u, buf.size());
    EXPECT_EQ(16u, buf.capacity());
}

TEST(CopyOnWriteBufferTest, SetSizeDoesntDecreaseCapacity) {
    CopyOnWriteBuffer buf1(kTestData, 5, 10);
    CopyOnWriteBuffer buf2(buf1);

    buf2.Resize(2);

    EXPECT_EQ(2u, buf2.size());
    EXPECT_EQ(10u, buf2.capacity());
}

TEST(CopyOnWriteBufferTest, ClearDoesntChangeOriginal) {
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

TEST(CopyOnWriteBufferTest, ClearDoesntChangeCapacity) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);

    buf2.Clear();

    EXPECT_EQ(0u, buf2.size());
    EXPECT_EQ(10u, buf2.capacity());
}

TEST(CopyOnWriteBufferTest, DataAccessorDoesntCloneData) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);

    const uint8_t* data1 = buf1.cdata();
    const uint8_t* data2 = buf2.cdata();
    EXPECT_EQ(data1, data2);
}

TEST(CopyOnWriteBufferTest, MutableDataClonesDataWhenShared) {
    CopyOnWriteBuffer buf1(kTestData, 3, 10);
    CopyOnWriteBuffer buf2(buf1);
    const uint8_t* data = buf1.cdata();

    uint8_t* data1 = buf1.data();
    uint8_t* data2 = buf2.data();
    // buf1 was cloned above.
    EXPECT_NE(data1, data);
    // Therefore buf2 was no longer sharing data and was not cloned.
    EXPECT_EQ(data2, data);
}

} // namespace test
} // namespace naivertc