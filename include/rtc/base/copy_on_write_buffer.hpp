#ifndef _RTC_BASE_COPY_ON_WRITE_BUFFER_H_
#define _RTC_BASE_COPY_ON_WRITE_BUFFER_H_

#include "base/defines.hpp"

#include <memory>
#include <type_traits>

namespace naivertc {
namespace internal {

// Determines if type U are compatible with type T.
template <typename T, typename U>
struct IsCompatible {
  static constexpr bool value =
        // 1. Forbib top-level volatile
        !std::is_volatile<U>::value &&
        // 2. if type U are compatible with type T:
        // a) First, both of them are byte-sized integers, e.g.: char, int8_t, and uint8_t
        // b) Otherwise, Ignoring top-level const, they are same type.
        ((std::is_integral<T>::value && sizeof(T) == 1)
           ? (std::is_integral<U>::value && sizeof(U) == 1)
           : (std::is_same<T, typename std::remove_const<U>::type>::value));
};

} // internal

class RTC_CPP_EXPORT CopyOnWriteBuffer {
public:
    CopyOnWriteBuffer();
    CopyOnWriteBuffer(const CopyOnWriteBuffer&);
    CopyOnWriteBuffer(CopyOnWriteBuffer &&);

    CopyOnWriteBuffer(const BinaryBuffer&);
    CopyOnWriteBuffer(BinaryBuffer &&);

    explicit CopyOnWriteBuffer(size_t size);
    CopyOnWriteBuffer(size_t size, size_t capacity);

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    CopyOnWriteBuffer(const T* begin, const T* end)
        : CopyOnWriteBuffer(begin, ptrdiff_t(end - begin)) {}

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    CopyOnWriteBuffer(const T* data, size_t size)
        : CopyOnWriteBuffer(data, size, size) {}

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    CopyOnWriteBuffer(const T* data, size_t size, size_t capacity) 
        : CopyOnWriteBuffer(size, capacity) {
        if (buffer_) {
            std::memcpy(buffer_->data(), data, size);
        }
    }

    template <typename T,
              size_t N,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    CopyOnWriteBuffer(const T (&array)[N]) : CopyOnWriteBuffer(array, N) {}

    template <typename T,
        // Container has data and size
        typename std::enable_if<
            std::is_convertible<decltype(std::declval<T>().data()), uint8_t*>::value &&
            std::is_convertible<decltype(std::declval<T>().size()), std::size_t>::value>::type* = nullptr>
    CopyOnWriteBuffer(const T& t) : CopyOnWriteBuffer(t.data(), t.size()) {}

    CopyOnWriteBuffer& operator=(const CopyOnWriteBuffer& other);
    CopyOnWriteBuffer& operator=(CopyOnWriteBuffer&& other);
   
    virtual ~CopyOnWriteBuffer();

    const uint8_t* data() const;
    const uint8_t* cdata() const;
    uint8_t* data();

    size_t size() const;
    size_t capacity() const;
    bool empty() const { return size() == 0; }

    bool operator==(const CopyOnWriteBuffer& other) const;
    bool operator!=(const CopyOnWriteBuffer& other) const {
        return !(*this == other);
    }

    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;

    uint8_t& at(size_t index);
    const uint8_t& at(size_t index) const;

    std::vector<uint8_t>::iterator begin();
    std::vector<uint8_t>::iterator end();
    std::vector<uint8_t>::const_iterator cbegin() const;
    std::vector<uint8_t>::const_iterator cend() const;

    std::vector<uint8_t>::reverse_iterator rbegin();
    std::vector<uint8_t>::reverse_iterator rend();
    std::vector<uint8_t>::const_reverse_iterator crbegin() const;
    std::vector<uint8_t>::const_reverse_iterator crend() const;

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    void Assign(const T* data, size_t size) {
        if (!buffer_) {
            buffer_ = size > 0 ? std::make_shared<BinaryBuffer>(data, data + size) : nullptr;
        }else if (buffer_.use_count() == 1) {
            if (size > buffer_->capacity()) {
                buffer_->reserve(size);
            }
            buffer_->assign(data, data + size);
        }else {
            size_t capacity = std::max(buffer_->capacity(), size);
            buffer_ = std::make_shared<BinaryBuffer>(data, data + size);
            buffer_->reserve(capacity);
        }
    }

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    void Assign(const T* begin, const uint8_t* end) {
        auto size = ptrdiff_t(end - begin);
        assert(size >= 0);
        Assign(begin, size);
    }

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    void Append(const T* data, size_t size) {
        if (!buffer_) {
            buffer_ = size > 0 ? std::make_shared<BinaryBuffer>(data, data + size) : nullptr;
            return;
        }
        CloneIfNecessary(buffer_->capacity());
        buffer_->insert(buffer_->end(), data, data + size);
    }

    template <typename T,
              size_t N,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    void Append(const T (&array)[N]) {
        Append(array, N);
    }

    void Append(std::vector<uint8_t>::const_iterator begin, 
                std::vector<uint8_t>::const_iterator end);

    template <typename T,
              typename std::enable_if<internal::IsCompatible<uint8_t, T>::value>::type* = nullptr>
    void Insert(std::vector<uint8_t>::iterator pos, const T* data, size_t size) {
        assert(buffer_ != nullptr);
        CloneIfNecessary(buffer_->capacity());
        buffer_->insert(pos, data, data + size);
    }

    void Insert(std::vector<uint8_t>::iterator pos, 
                std::vector<uint8_t>::const_iterator begin, 
                std::vector<uint8_t>::const_iterator end);
    void Resize(size_t size);
    void Clear();

    void Swap(CopyOnWriteBuffer& other);

    void EnsureCapacity(size_t new_capacity);

private:
    void CloneIfNecessary(size_t new_capacity);
    void CreateEmptyBufferIfNecessary();
private:
    std::shared_ptr<BinaryBuffer> buffer_ = nullptr;
};

} // namespace naivertc

#endif