#include "task_queue_examples.hpp"

// naivertc
#include <common/logger.hpp>

#include <future>
#include <iostream>

namespace taskqueue {

Example::Example() {}

Example::~Example() {
    std::cout  << __FUNCTION__ << std::endl;
}
    
void Example::DelayPost() {
    auto start = boost::posix_time::second_clock::universal_time();
    task_queue_.AsyncAfter(5, [this, start](){
        if (task_queue_.is_in_current_queue()) {
            std::cout  << "in the same queue." << std::endl;
        }else {
            std::cout  << "in the other queue." << std::endl;
        }
        auto end = boost::posix_time::second_clock::universal_time();
        auto delay_in_sec = end - start;
        std::cout  << "Delay in sec: " << delay_in_sec.seconds() << std::endl;
    });
    std::cout  << "Did async post" << std::endl;
}

void Example::Post() {
    task_queue_.Async([this](){
        if (task_queue_.is_in_current_queue()) {
            std::cout  << "in the same queue." << std::endl;
        }
    });
}


} // namespace taskqueue
