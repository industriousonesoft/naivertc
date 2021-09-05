#include "task_queue_examples.hpp"
// #include "volatile_examples.hpp"
#include "sdp_description_examples.hpp"

// naivertc
#include <common/logger.hpp>

// boost
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

#include <iostream>
#include <memory>

int main(int argc, const char* argv[]) {

    naivertc::logging::InitLogger(naivertc::logging::Level::VERBOSE);

    boost::asio::io_context ioc;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioc.get_executor());

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        ioc.stop();
        std::cout << "main ioc exit" << std::endl;
    });

    std::cout << "test start" << std::endl;

    // TaskQueue
    std::unique_ptr<taskqueue::Example> task_queue_example = std::make_unique<taskqueue::Example>();
    // task_queue_example->DelayPost();
    // task_queue_example->Post();
    task_queue_example->RepeatingTask(3);
    // task_queue_example.reset();

    // // Volatile
    // // volatile_tests::WithoutVolatile();

    // // sdp
    // sdptest::BuildAnOffer();
    // sdptest::ParseAnAnswer();

    ioc.run();

    std::cout << "test ended" << std::endl;

    return 0;
}