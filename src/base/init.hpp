#ifndef _BASE_INIT_H_
#define _BASE_INIT_H_

#include "base/defines.hpp"

namespace naivertc {

// Log level
enum class LoggingLevel {
    NONE,
    DEBUG,
    WARNING,
    INFO,
    ERROR,
    VERBOSE
};

void Init(LoggingLevel level = LoggingLevel::NONE);
void Cleanup();
    
} // namespace naivertc


#endif