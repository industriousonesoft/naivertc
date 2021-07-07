#include "task_queue_tests.hpp"

// naivertc
#include <common/logger.hpp>

// boost
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

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
    // taskqueue::DelayPostTest();
    taskqueue::PostTest();


    ioc.run();

    return 0;
}