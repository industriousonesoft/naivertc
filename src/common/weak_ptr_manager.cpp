#include "common/weak_ptr_manager.hpp"

namespace naivertc {
WeakPtrManager::WeakPtrManager() {

}

WeakPtrManager::~WeakPtrManager() {
    ptr_set_.clear();
}

void WeakPtrManager::Register(void* ptr) {
    // 排他锁：一读一写
    std::unique_lock lock(mutex_);
    if (ptr == nullptr) return;
    ptr_set_.insert(ptr);
}

void WeakPtrManager::Deregister(void* ptr) {
    std::unique_lock lock(mutex_);
    if (ptr == nullptr) return;
    ptr_set_.erase(ptr);
}

std::optional<std::shared_lock<std::shared_mutex>> WeakPtrManager::TryLock(void* ptr) {
    // 共享锁: 多读一写
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!ptr) return std::nullopt;
    return ptr_set_.find(ptr) != ptr_set_.end() ? std::make_optional(std::move(lock)) : std::nullopt;
}

}