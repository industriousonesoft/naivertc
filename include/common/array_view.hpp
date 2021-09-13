#ifndef _RTC_COMMON_ARRAY_VIEW_H_
#define _RTC_COMMON_ARRAY_VIEW_H_

#include "base/defines.hpp"

#include <cstdint>
#include <vector>
#include <type_traits>

namespace naivertc {

template<typename T>
class RTC_CPP_EXPORT ArrayView {
public:
    using value_type = T;
    using const_iterator = const T*;

    ArrayView(T* ptr, size_t size) noexcept : ptr_(ptr), size_(size) {}

    ArrayView() noexcept : ArrayView(nullptr, 0) {}
    ArrayView(std::nullptr_t) noexcept : ArrayView() {}
  
    template <typename U, size_t N>
    ArrayView(U (&buffer)[N]) noexcept : ArrayView(buffer, N) {}

    // ArrayView<T> to ArraryView<const T>
    // std::vector<T> to ArraryView<const T> or ArraryView<T>
    template <
        typename U,
        // Container has data and size
        typename std::enable_if<
            std::is_convertible<decltype(std::declval<U>().data()), T*>::value &&
            std::is_convertible<decltype(std::declval<U>().size()), std::size_t>::value
        >::type* = nullptr
    >
    // FIXME: 通过右值引用实现T to const T的转换？
    ArrayView(const U& u) noexcept : ArrayView(u.data(), u.size()) {}

    T& operator[](int i) noexcept { return ptr_[i]; }
    const T& operator[](int i) const noexcept { return  ptr_[i]; }
    T* data() const noexcept { return ptr_; }
    size_t size() const noexcept { return size_; };
    bool empty() const noexcept { return this->size() == 0; }

    T* begin() const noexcept { return this->data(); }
    T* end() const noexcept { return this->data() + this->size(); }
    const T* cbegin() const { return this->data(); }
    const T* cend() const { return this->data() + this->size(); }

    std::reverse_iterator<T*> rbegin() const {
        return std::make_reverse_iterator(end());
    }
    std::reverse_iterator<T*> rend() const {
        return std::make_reverse_iterator(begin());
    }
    std::reverse_iterator<const T*> crbegin() const {
        return std::make_reverse_iterator(cend());
    }
    std::reverse_iterator<const T*> crend() const {
        return std::make_reverse_iterator(cbegin());
    }

    ArrayView<T> subview(size_t offset, size_t size) const noexcept { 
        return offset < this->size() ? ArrayView<T>(this->data() + offset, std::min(size, this->size() - offset))
                                     : ArrayView<T>(nullptr, 0);
    }

    ArrayView<T> subview(size_t offset) const noexcept { 
        return subview(offset, this->size());
    }

    void Reset() {
        ptr_ = nullptr;
        size_ = 0;
    }

private:
    T* ptr_;
    size_t size_;
};

} // namespace naivertc

#endif