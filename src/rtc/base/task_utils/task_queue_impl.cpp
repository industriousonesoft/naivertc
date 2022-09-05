#include "rtc/base/task_utils/task_queue_impl.hpp"
#include "testing/simulated_task_queue.hpp"

namespace naivertc {
// Support `thread_local`
#if defined(RTC_SUPPORT_THREAD_LOCAL)

namespace {

RTC_CONST_INIT thread_local TaskQueueImpl* current_task_queue = nullptr;

} // namespace

TaskQueueImpl* TaskQueueImpl::Current() {
    return current_task_queue;
}

TaskQueueImpl::CurrentTaskQueueSetter::CurrentTaskQueueSetter(TaskQueueImpl* task_queue) 
    : previous_(current_task_queue) {
    current_task_queue = task_queue;
}

TaskQueueImpl::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
    current_task_queue = previous_;
}

// Support TLS and POSIX platfrom
#elif defined(RTC_SUPPORT_TLS) && defined(NAIVERTC_POSIX)
#include <pthread.h>
// Emscripten does not support the C++11 thread_local keyword but does support
// the pthread TLS(Thread-local storage) API.
// https://github.com/emscripten-core/emscripten/issues/3502
namespace {

RTC_CONST_INIT pthread_key_t current_task_queue = 0;

void InitializeTls() {
    assert(pthread_key_create(&current_task_queue, nullptr) == 0);
}

pthread_key_t GetTlsKey() {
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    assert(pthread_once(&init_once, &InitializeTls) == 0);
    return current_task_queue;
}

TaskQueueImpl* GetCurrentTaskQueue() {
    return static_cast<TaskQueueImpl*>(pthread_getspecific(GetTlsKey()));
}

void SetCurrentTaskQueue(TaskQueueImpl* value) {
    pthread_setspecific(GetTlsKey(), value);
}

} // namespace

TaskQueueImpl::CurrentTaskQueueSetter::CurrentTaskQueueSetter(TaskQueueImpl* task_queue) 
    : previous_(GetCurrentTaskQueue()) {
     SetCurrentTaskQueue(task_queue);
}

TaskQueueImpl::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
    SetCurrentTaskQueue(previous_);
}

// Unsupported platform
#else
#error "Platform unsupport TLS(thread-local storage)."
#endif // defined(RTC_SUPPORT_THREAD_LOCAL)

} // namespace naivertc
