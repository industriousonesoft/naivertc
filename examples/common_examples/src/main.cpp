// #include "task_queue_examples.hpp"
// #include "volatile_examples.hpp"
#include "sdp_description_examples.hpp"

// naivertc
#include <common/logger.hpp>

#include <iostream>

int main(int argc, const char* argv[]) {

    std::cout << "test start" << std::endl;
    // // Logger
    naivertc::logging::InitLogger(naivertc::logging::Level::VERBOSE);

    // // TaskQueue
    // // taskqueue::DelayPostTest();
    // // taskqueue::PostTest();

    // // Volatile
    // // volatile_tests::WithoutVolatile();

    // // sdp
    // sdptest::BuildAnOffer();
    sdptest::ParseAnAnswer();

    std::cout << "test ended" << std::endl;

    return 0;
}