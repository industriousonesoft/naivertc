#ifndef _COMMON_PROXY_H_
#define _COMMON_PROXY_H_

#include <memory>

namespace naivertc {

template <typename T> using impl_ptr = std::shared_ptr<T>;
template <typename T> class Proxy {
public:
    Proxy(impl_ptr<T> impl) : impl_(std::move(impl)) {}
    template <typename... Args>
    Proxy(Args... args) : impl_(std::make_shared<T>(std::move(args)...)) {};
    // &&表示右值引用，const &表示常量
    // 当同时存在参数为右值引用和常量的重载函数1，2时，传入常量时会优先选择2，没有再选1，而传入右值引用时，优先选择1，没有再选择2
    // 此处两个重载均实现，意思就是只支持参数为右值引用的拷贝函数
    Proxy(Proxy<T> && p) { *this = std::move(p); }
    Proxy(const Proxy<T> &) = delete;

    virtual ~Proxy() = default;

    Proxy &operator=(Proxy<T> && p) {
        impl_ = std::move(p.impl_);
        return *this;
    }

    Proxy &operator=(const Proxy<T> &) = delete;

protected:
    impl_ptr<T> impl() { return impl_; }
    impl_ptr<const T> impl() const { return impl_; }

private:
    impl_ptr<T> impl_;
};

}

#endif