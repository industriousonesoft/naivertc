#include "task_queue_examples.hpp"

#include <future>
#include <iostream>

namespace taskqueue {

Example::Example() 
    : clock_(std::make_shared<naivertc::RealTimeClock>()),
      task_queue_(std::make_shared<naivertc::TaskQueue>()),
      last_execution_time_(naivertc::Timestamp::Seconds(0)) {
    repeating_task_ = std::make_shared<naivertc::RepeatingTask>(clock_, task_queue_, [&](){
        naivertc::Timestamp current_time = clock_->CurrentTime();
        if (!last_execution_time_.IsZero()) {
            std::cout << "Repeating task: " << ( current_time - last_execution_time_).seconds() << " s "<< std::endl;
        }
        last_execution_time_ = current_time;
        std::cout << "Executed task." << std::endl;
        return 3;
    });
}

Example::~Example() {
    std::cout  << __FUNCTION__ << std::endl;
}
    
void Example::DelayPost() {
    auto start = boost::posix_time::second_clock::universal_time();
    task_queue_->AsyncAfter(5, [this, start](){
        if (task_queue_->is_in_current_queue()) {
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
    task_queue_->Async([this](){
        if (task_queue_->is_in_current_queue()) {
            std::cout  << "in the same queue." << std::endl;
        }
    });
}

void Example::RepeatingTask(TimeInterval delay) {
    repeating_task_->Start(delay);
}

} // namespace taskqueue
