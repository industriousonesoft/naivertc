// naivertc
#include <common/task_queue.hpp>
#include <common/logger.hpp>

// boost
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

#include <future>

static naivertc::TaskQueue task_queue;

void TaskQueueDelayPostTest() {
    
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

void TaskQueuePostTest() {
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

int main(int argc, const char* argv[]) {

    // Logger
    naivertc::logging::InitLogger(naivertc::logging::Level::VERBOSE);

    boost::asio::io_context ioc;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioc.get_executor());

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        ioc.stop();
        PLOG_DEBUG << "main ioc exit";
    });

    // TaskQueue
    // TaskQueueDelayPostTest();
    TaskQueuePostTest();


    ioc.run();

    return 0;
}