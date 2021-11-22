#ifndef _BASE_THREAD_ANNOTATION_H_
#define _BASE_THREAD_ANNOTATION_H_

// SWIG: Simplified Wrapper and Interface Generator
#if defined(__clang__) && (!defined(SWIG))
#define RTC_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define RTC_THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#endif

// Document if a shared variable/field needs to be protected by a lock.
// GUARDED_BY allows the user to specify a particular lock that should be
// held when accessing the annotated variable.
#define RTC_GUARDED_BY(x) RTC_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

// Document if the memory location pointed to by a pointer should be guarded
// by a lock when dereferencing the pointer. Note that a pointer variable to a
// shared memory location could itself be a shared variable. For example, if a
// shared global pointer q, which is guarded by mu1, points to a shared memory
// location that is guarded by mu2, q should be annotated as follows:
//     int *q GUARDED_BY(mu1) PT_GUARDED_BY(mu2);
#define RTC_PT_GUARDED_BY(x) RTC_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

// Document the acquisition order between locks that can be held
// simultaneously by a thread. For any two locks that need to be annotated
// to establish an acquisition order, only one of them needs the annotation.
// (i.e. You don't have to annotate both locks with both ACQUIRED_AFTER
// and ACQUIRED_BEFORE.)
#define RTC_ACQUIRED_AFTER(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(x))
#define RTC_ACQUIRED_BEFORE(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(x))

// The following three annotations document the lock requirements for
// functions/methods.

// Document if a function expects certain locks to be held before it is called
#define RTC_EXCLUSIVE_LOCKS_REQUIRED(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))
#define RTC_SHARED_LOCKS_REQUIRED(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

// Document the locks acquired in the body of the function. These locks
// cannot be held when calling this function (as google3's Mutex locks are
// non-reentrant).
#define RTC_LOCKS_EXCLUDED(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

  // Document the lock the annotated function returns without acquiring it.
#define RTC_LOCK_RETURNED(x) RTC_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

// Document if a class/type is a lockable type (such as the Mutex class).
#define RTC_LOCKABLE RTC_THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// Document if a class is a scoped lockable type (such as the MutexLock class).
#define RTC_SCOPED_LOCKABLE RTC_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

// The following annotations specify lock and unlock primitives.
#define RTC_EXCLUSIVE_LOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

#define RTC_SHARED_LOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

#define RTC_EXCLUSIVE_TRYLOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

#define RTC_SHARED_TRYLOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(shared_trylock_function(__VA_ARGS__))

#define RTC_UNLOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

#endif