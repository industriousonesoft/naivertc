#include "task_queue_examples.hpp"
// #include "volatile_examples.hpp"
#include "sdp_description_examples.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/transports/sctp_message.hpp"
#include "common/utils_random.hpp"

// naivertc
#include <common/logger.hpp>

// boost
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <limits>
#include <set>

template<typename T>
struct Compare {
    bool operator()(T a, T b) const {
        return b > a;
    }
};

int main(int argc, const char* argv[]) {

    naivertc::logging::InitLogger(naivertc::logging::Level::VERBOSE);

    boost::asio::io_context ioc;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioc.get_executor());

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        ioc.stop();
        std::cout << "main ioc exit" << std::endl;
    });

    // std::cout << std::numeric_limits<uint32_t>::max() << " - " << ((static_cast<int64_t>(1) << 32) - 1) << std::endl;

    std::cout << "test start" << std::endl;

    // Sequence number with compare operator
    std::cout << "Squence numbers: " << std::endl;
    std::set<uint16_t, Compare<uint16_t>> seq_nums = {11, 666, 444, 22, 33, 555};
    for (uint16_t val : seq_nums) {
        std::cout << val << " - ";
    }
    std::cout << std::endl;

    // TaskQueue
    // std::unique_ptr<taskqueue::Example> task_queue_example = std::make_unique<taskqueue::Example>();
    // task_queue_example->DelayPost();
    // task_queue_example->Post();
    // task_queue_example->TestRepeatingTask();
    // task_queue_example.reset();

    // CopyOnWriteBuffer
    // naivertc::CopyOnWriteBuffer buf1(10);
    // auto buf2 = buf1; // Copy
    // naivertc::CopyOnWriteBuffer buf3(buf1); // Copy
    // auto buf4 = std::move(buf2); // Move
    // naivertc::CopyOnWriteBuffer buf5(std::move(buf3)); // Move
    // naivertc::TaskQueue task_queue;
    // task_queue.Async([&buf1]() {
    //     std::cout << "Reference with no copy or move" << std::endl;
    // });
    // task_queue.Async([buf6=std::move(buf5) /* move */]() {
        
    // });
    // buf2 = buf1; // Copy =
    // buf4 = std::move(buf2); // Move =

    // SctpMessageToSend
    // std::optional<naivertc::SctpMessageToSend> test_buffer_opt = std::nullopt;
    // naivertc::SctpMessageToSend message(naivertc::SctpMessageToSend::Type::STRING, 0, naivertc::CopyOnWriteBuffer(100), naivertc::SctpMessageToSend::Reliability());
    // message.Advance(55);
    // test_buffer_opt.emplace(std::move(message));
    // PLOG_DEBUG << "Test buffer offset: " << test_buffer_opt.value().available_payload_size();

    // sdp
    // sdptest::BuildAnOffer();
    // sdptest::ParseAnAnswer();

    // Random string
    // std::cout << "16 length random string: " << naivertc::utils::random::random_string(16) << std::endl;
    // std::cout << "36 length random string: " << naivertc::utils::random::random_string(36) << std::endl;


    ioc.run();

    std::cout << "test ended" << std::endl;

    return 0;
}