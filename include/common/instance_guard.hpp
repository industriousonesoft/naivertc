#ifndef _COMMON_INSTANCE_GUARD_H_
#define _COMMON_INSTANCE_GUARD_H_

#include "base/defines.hpp"

#include <unordered_set>
#include <shared_mutex>
#include <optional>

namespace naivertc {

template <class T>
class RTC_CPP_EXPORT InstanceGuard {
public:
    InstanceGuard();
    ~InstanceGuard();

    void Add(T* ins);
    void Remove(T* ins);

    std::optional<std::shared_lock<std::shared_mutex>> TryLock(T* ins);
private:
    std::unordered_set<T*> instance_set_;
    std::shared_mutex mutex_; 
};

}

#endif