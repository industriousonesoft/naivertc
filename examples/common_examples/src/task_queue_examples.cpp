#include "task_queue_examples.hpp"

// naivertc
#include <common/task_queue.hpp>
#include <common/logger.hpp>

#include <future>
#include <iostream>

namespace taskqueue {
    
static naivertc::TaskQueue task_queue;

void DelayPostTest() {
    
    auto start = boost::posix_time::second_clock::universal_time();
    task_queue.PostDelay(5, [start](){
        if (task_queue.is_in_current_queue()) {
            std::cout  << "in the same queue." << std::endl;
        }else {
            std::cout  << "in the other queue." << std::endl;
        }
        auto end = boost::posix_time::second_clock::universal_time();
        auto delay_in_sec = end - start;
        std::cout  << "delay_in_sec: " << delay_in_sec.seconds() << std::endl;
    });
}

void PostTest() {
    std::promise<bool> promise;
    auto future = promise.get_future(); 
    std::cout  << "Post started." << std::endl;
    task_queue.Post([&promise](){
        promise.set_value(true);
        std::cout  << "Post in progress." << std::endl;
    });
    future.get();
    std::cout  << "Post ended." << std::endl;
}

} // namespace taskqueue
