#include "task_queue_examples.hpp"
#include "volatile_examples.hpp"

// naivertc
#include <common/logger.hpp>

int main(int argc, const char* argv[]) {

    // Logger
    naivertc::logging::InitLogger(naivertc::logging::Level::VERBOSE);

    // TaskQueue
    // taskqueue::DelayPostTest();
    // taskqueue::PostTest();

    // Volatile
    // volatile_tests::WithoutVolatile();

    return 0;
}