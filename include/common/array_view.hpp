#ifndef _RTC_COMMON_ARRAY_VIEW_H_
#define _RTC_COMMON_ARRAY_VIEW_H_

#include "base/defines.hpp"

#include <cstdint>

namespace naivertc {

template<typename T>
class RTC_CPP_EXPORT ArrayView {
public:
    ArrayView(T* ptr, size_t size) noexcept : ptr_(ptr), size_(size) {}

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