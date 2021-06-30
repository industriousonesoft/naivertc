#include "common/instance_guard.hpp"

namespace naivertc {
template <class T>
InstanceGuard<T>::InstanceGuard() {

}

template <class T>
InstanceGuard<T>::~InstanceGuard() {
    instance_set_.clear();
}

template <class T>
void InstanceGuard<T>::Add(T* ins) {
    std::unique_lock lock(mutex_);
    instance_set_.insert(ins);
}

template <class T>
void InstanceGuard<T>::Remove(T* ins) {
    std::unique_lock lock(mutex_);
    instance_set_.erase(ins);
}

template <class T>
std::optional<std::shared_lock<std::shared_mutex>> InstanceGuard<T>::TryLock(T* ins) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return instance_set_.find(ins) != instance_set_.end() ? std::make_optional(std::move(lock)) : std::nullopt;
}

}