#ifndef _COMMON_WEAK_PTR_MANAGER_H_
#define _COMMON_WEAK_PTR_MANAGER_H_

#include "base/defines.hpp"

#include <unordered_set>
#include <shared_mutex>
#include <optional>

namespace naivertc {

class WeakPtrManager {
public:
    static WeakPtrManager* SharedInstance() {
        // 使用静态局部变量来完全避免手动释放所分配的资源和线程安全问题
        // (c++11新标准支持静态局部变量安全访问)
        static WeakPtrManager instance;
        return &instance;
    }

    void Register(void* ptr);
    void Deregister(void* ptr);

    std::optional<std::shared_lock<std::shared_mutex>> Lock(void* ptr);

private:
    WeakPtrManager();
    ~WeakPtrManager();
private:
    std::unordered_set<void*> ptr_set_;
    std::shared_mutex mutex_; 
};

}

#endif