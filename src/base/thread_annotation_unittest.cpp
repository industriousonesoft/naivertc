#include "base/thread_annotation.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

class RTC_LOCKABLE Lock {
public:
    void EnterWrite() const RTC_EXCLUSIVE_LOCK_FUNCTION() {}
    void EnterRead() const RTC_SHARED_LOCK_FUNCTION() {}
    bool TryEnterWrite() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
        return true;
    }
    bool TryEnterRead() const RTC_SHARED_TRYLOCK_FUNCTION(true) {
        return true;
    }
    void Leave() const RTC_UNLOCK_FUNCTION() {}
};

class RTC_SCOPED_LOCKABLE ScopeLock {
public:
    explicit ScopeLock(const Lock& lock) RTC_EXCLUSIVE_LOCK_FUNCTION(lock) {}
    ~ScopeLock() RTC_UNLOCK_FUNCTION() {}
};

class ThreadSafe {
public:
    ThreadSafe() { pt_protected_by_lock_ = new int; }

    ~ThreadSafe() { delete pt_protected_by_lock_; }

    void LockInOrder() {
        beforelock_.EnterWrite();
        lock_.EnterWrite();
        pt_lock_.EnterWrite();

        pt_lock_.Leave();
        lock_.Leave();
        beforelock_.Leave();
    }

    void UnprotectedFunction() RTC_LOCKS_EXCLUDED(lock_, pt_lock_) {
        // Can access unprotected Value.
        unprotected_ = 15;
        // Can access pointers themself, but not data they point to.
        int* tmp = pt_protected_by_lock_;
        pt_protected_by_lock_ = tmp;
    }

    void ReadProtected() {
        lock_.EnterRead();
        unprotected_ = protected_by_lock_;
        lock_.Leave();

        if (pt_lock_.TryEnterRead()) {
        unprotected_ = *pt_protected_by_lock_;
        pt_lock_.Leave();
        }
    }

    void WriteProtected() {
        lock_.EnterWrite();
        protected_by_lock_ = unprotected_;
        lock_.Leave();

        if (pt_lock_.TryEnterWrite()) {
        *pt_protected_by_lock_ = unprotected_;
        pt_lock_.Leave();
        }
    }

    void CallReadProtectedFunction() {
        lock_.EnterRead();
        pt_lock_.EnterRead();
        ReadProtectedFunction();
        pt_lock_.Leave();
        lock_.Leave();
    }

    void CallWriteProtectedFunction() {
        ScopeLock scope_lock(GetLock());
        ScopeLock pt_scope_lock(pt_lock_);
        WriteProtectedFunction();
    }

private:
    void ReadProtectedFunction() RTC_SHARED_LOCKS_REQUIRED(lock_, pt_lock_) {
        unprotected_ = protected_by_lock_;
        unprotected_ = *pt_protected_by_lock_;
    }

    void WriteProtectedFunction() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_, pt_lock_) {
        int x = protected_by_lock_;
        *pt_protected_by_lock_ = x;
        protected_by_lock_ = unprotected_;
    }

    const Lock& GetLock() RTC_LOCK_RETURNED(lock_) { return lock_; }

    Lock beforelock_ RTC_ACQUIRED_BEFORE(lock_);
    Lock lock_;
    Lock pt_lock_ RTC_ACQUIRED_AFTER(lock_);

    int unprotected_ = 0;

    int protected_by_lock_ RTC_GUARDED_BY(lock_) = 0;

    int* pt_protected_by_lock_ RTC_PT_GUARDED_BY(pt_lock_);
};

MY_TEST(ThreadAnnotationsTest, Test) {
    // This test ensure thread annotations doesn't break compilation.
    // Thus no run-time expectations.
    ThreadSafe t;
    t.LockInOrder();
    t.UnprotectedFunction();
    t.ReadProtected();
    t.WriteProtected();
    t.CallReadProtectedFunction();
    t.CallWriteProtectedFunction();
}
    
} // namespace test
} // namespace naivertc
