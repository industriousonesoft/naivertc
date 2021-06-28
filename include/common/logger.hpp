#ifndef _COMMON_LOGGER_H_
#define _COMMON_LOGGER_H_

#include "base/defines.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace logging {

enum class Level { // Don't change, it MUST match plog secerity
    NONE = 0,
	FATAL = 1,
	ERROR = 2,
	WARNING = 3,
	INFO = 4,
	DEBUG = 5,
	VERBOSE = 6
};

using LoggingCallback = std::function<bool(Level level, std::string message)>;

RTC_CPP_EXPORT void InitLogger(Level level, LoggingCallback callback = nullptr);
RTC_CPP_EXPORT void InitLogger(plog::Severity severity, plog::IAppender *appender = nullptr);

}

}

#endif