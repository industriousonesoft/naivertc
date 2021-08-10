#ifndef _BASE_DEFINES_H_
#define _BASE_DEFINES_H_

#include <cstdint>
#include <vector>

#ifndef RTC_CPP_EXPORT
#define RTC_CPP_EXPORT
#endif

#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
    TypeName(const TypeName&) = delete;     \
    TypeName& operator=(const TypeName&) = delete

using TimeInterval = long;
using BinaryBuffer = std::vector<uint8_t>;

// TODO: Overload Pattern in C++17，overload原理就是模板推导和转发，变参模板怎么理解？
/** eg:
struct overloadInt{ 
    void operator(int arg){
        std::cout<<arg<<' ';
    } 
};
struct overload : overloadInt{
    using overloadInt::operator();
};
*/
// overloaded helper
template <class... Ts> RTC_CPP_EXPORT struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// weak_bind
// WARNING: weak_bind DO NOT call in a constructor, since weak_from_this() is NOT allowed used in a constructor.
template <typename F, typename T, typename... Args> 
RTC_CPP_EXPORT auto weak_bind(F&& f, T* t, Args&& ..._args) {
    return [bound = std::bind(f, t, _args...), weak_this = t->weak_from_this()](auto &&...args) {
        if (auto shared_this = weak_this.lock()) {
            return bound(args...);
        }else {
            return static_cast<decltype(bound(args...))>(false);
        }
    };
}

#endif



