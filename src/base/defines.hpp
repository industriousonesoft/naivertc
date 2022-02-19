#ifndef _BASE_DEFINES_H_
#define _BASE_DEFINES_H_

#include <cstdint>
#include <vector>

// DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
    TypeName(const TypeName&) = delete;     \
    TypeName& operator=(const TypeName&) = delete;

// RTC_NOTREACHED
#define RTC_NOTREACHED() \
    assert(false && "NOT REACHED")

// FIXME: 暂不能使用uin64_t，因为unit_base中使用的是int64_t类型，如果使用uin64_t会导致越界问题
using TimeInterval = int64_t;
using BinaryBuffer = std::vector<uint8_t>;

// weak_bind
// WARNING: weak_bind DO NOT call in a constructor, since weak_from_this() is NOT allowed used in a constructor.
template <typename F, typename T, typename... Args> 
auto weak_bind(F&& f, T* t, Args&& ..._args) {
    return [bound = std::bind(f, t, _args...), weak_this = t->weak_from_this()](auto &&...args) {
        if (auto shared_this = weak_this.lock()) {
            return bound(args...);
        } else {
            return static_cast<decltype(bound(args...))>(false);
        }
    };
}

#endif



