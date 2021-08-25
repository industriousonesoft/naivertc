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
    T const& operator[](int i) const noexcept { return  ptr_[i]; }
    auto data() const noexcept { return ptr_; }
    auto size() const noexcept { return size_; };

    auto begin() noexcept { return ptr_; }
    auto end() noexcept { return ptr_ + size_; }

    ArrayView<T> subview(size_t offset, size_t size) const noexcept { 
        return offset < this->size() ? ArrayView<T>(this->data() + offset, std::min(size, this->size() - offset))
                                     : ArrayView<T>(nullptr, 0);
    }

private:
    T* ptr_;
    size_t size_;
};

} // namespace naivertc

#endif