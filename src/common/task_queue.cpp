#include "common/task_queue.hpp"

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>  

namespace naivertc {

TaskQueue::TaskQueue() 
    : work_guard_(boost::asio::make_work_guard(ioc_)),
      strand_(ioc_) {
    ioc_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_context::run, &ioc_)));
    ioc_thread_->detach();
}

TaskQueue::~TaskQueue() {
    ioc_.stop();
    // ioc_thread_ will exist after ioc stoped.
    ioc_thread_.reset();
}

void TaskQueue::Post(std::function<void()> handler) const {
    boost::asio::post(strand_, handler);
}

void TaskQueue::Dispatch(std::function<void()> handler) const {
    boost::asio::dispatch(strand_, handler);
}

void TaskQueue::PostDelay(TimeInterval delay_in_sec, std::function<void()> handler) {
    Post([this, delay_in_sec, handler](){
        // Construct a timer without setting an expiry time.
        boost::asio::deadline_timer timer(ioc_);
        // Set an expiry time relative to now.
        timer.expires_from_now(boost::posix_time::seconds(delay_in_sec));
        // Start an asynchronous wait
        timer.async_wait([this, handler](const boost::system::error_code& error){
            if (this->is_in_current_queue()) {
                handler();
            }else {
                Post(handler);
            }
        });
    });
}

bool TaskQueue::is_in_current_queue() const {
    return ioc_thread_->get_id() == boost::this_thread::get_id();    
}

}