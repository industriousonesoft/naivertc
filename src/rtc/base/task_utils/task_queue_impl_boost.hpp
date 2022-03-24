#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_BOOST_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_BOOST_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

#include <list>
#include <string>

namespace naivertc {

class TaskQueueImplBoost final : public TaskQueueImpl {
public:
    static std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> Create(std::string name);
public:
    void Delete() override;
    void Post(std::unique_ptr<QueuedTask> task) override;
    void PostDelayed(TimeDelta delay, std::unique_ptr<QueuedTask> task) override;

private:
    // Users of the TaskQueue should call Create instead of 
    // directly create this instance.
    TaskQueueImplBoost(std::string name);
    // Users of the TaskQueue should call Delete instead of 
    // directly deleting this instance.
    ~TaskQueueImplBoost() override;

    struct ScopedQueuedTask;
    void ScheduleTaskAfter(TimeDelta delay, ScopedQueuedTask&& scoped_task);

private:
    // ScopedQueuedTask
    struct ScopedQueuedTask {
    public: 
        ScopedQueuedTask(std::unique_ptr<QueuedTask> queued_task) 
            : queued_task_(std::move(queued_task)) {}

        void operator()() {
            if (queued_task_) {
                queued_task_->Run();
            }
        }   

    private:
        std::unique_ptr<QueuedTask> queued_task_;
    };

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;

    std::list<boost::asio::deadline_timer*> pending_timers_;
};
    
} // namespace naivertc


#endif