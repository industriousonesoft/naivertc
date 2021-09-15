#include "task_queue_examples.hpp"
// #include "volatile_examples.hpp"
#include "sdp_description_examples.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

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
    // std::unique_ptr<taskqueue::Example> task_queue_example = std::make_unique<taskqueue::Example>();
    // task_queue_example->DelayPost();
    // task_queue_example->Post();
    // task_queue_example->TestRepeatingTask();
    // task_queue_example.reset();

    // CopyOnWriteBuffer
    naivertc::CopyOnWriteBuffer buf1(10);
    auto buf2 = buf1; // Copy
    naivertc::CopyOnWriteBuffer buf3(buf1); // Copy
    auto bu4 = std::move(buf2); // Move
    naivertc::CopyOnWriteBuffer buf5(std::move(buf3)); // Move
    naivertc::TaskQueue task_queue;
    task_queue.Async([buf6=std::move(buf1) /* move */]() {
        
    });
    task_queue.Async([&buf1]() {
    });

    // sdp
    // sdptest::BuildAnOffer();
    // sdptest::ParseAnAnswer();

    ioc.run();

    std::cout << "test ended" << std::endl;

    return 0;
}