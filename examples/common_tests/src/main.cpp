// naivertc
#include <common/task_queue.hpp>
#include <common/logger.hpp>

// boost
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

void TaskQueueTest() {
    naivertc::TaskQueue task_queue;
    auto start = boost::posix_time::second_clock::universal_time();
    task_queue.PostDelay(5, [&task_queue, start](){
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
    TaskQueueTest();

    ioc.run();

    return 0;
}