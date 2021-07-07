#include "task_queue_examples.hpp"

// naivertc
#include <common/task_queue.hpp>
#include <common/logger.hpp>

#include <future>

namespace taskqueue {
    
static naivertc::TaskQueue task_queue;

void DelayPostTest() {
    
    auto start = boost::posix_time::second_clock::universal_time();
    task_queue.PostDelay(5, [start](){
        if (task_queue.is_in_current_queue()) {
            PLOG_DEBUG << "in the same queue.";
        }else {
            PLOG_DEBUG << "in the other queue.";
        }
        auto end = boost::posix_time::second_clock::universal_time();
        auto delay_in_sec = end - start;
        PLOG_DEBUG << "delay_in_sec: " << delay_in_sec.seconds();
    });
}

void PostTest() {
    std::promise<bool> promise;
    auto future = promise.get_future(); 
    PLOG_DEBUG << "Post started.";
    task_queue.Post([&promise](){
        promise.set_value(true);
        PLOG_DEBUG << "Post in progress.";
    });
    future.get();
    PLOG_DEBUG << "Post ended.";
}

} // namespace taskqueue
