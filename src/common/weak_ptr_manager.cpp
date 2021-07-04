#include "common/weak_ptr_manager.hpp"

namespace naivertc {
WeakPtrManager::WeakPtrManager() {

}

WeakPtrManager::~WeakPtrManager() {
    ptr_set_.clear();
}

void WeakPtrManager::Register(void* ptr) {
    if (ptr == nullptr) return;
    // 排他锁
    std::unique_lock lock(mutex_);
    ptr_set_.insert(ptr);
}

void WeakPtrManager::Deregister(void* ptr) {
    if (ptr == nullptr) return;
    std::unique_lock lock(mutex_);
    ptr_set_.erase(ptr);
}

std::optional<std::shared_lock<std::shared_mutex>> WeakPtrManager::TryLock(void* ptr) {
    // 共享锁
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return ptr_set_.find(ptr) != ptr_set_.end() ? std::make_optional(std::move(lock)) : std::nullopt;
}

}