#include "common/logger.hpp"

// plog
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <plog/Logger.h>


#include <mutex>

#ifdef _WIN32
#include <codecvt>
#include <locale>
#endif

namespace naivertc {
namespace logging {

struct LogAppender : public plog::IAppender {
	LoggingCallback callback;

	void write(const plog::Record &record) override {
		const auto severity = record.getSeverity();
		auto formatted = plog::FuncMessageFormatter::format(record);
		formatted.pop_back(); // remove newline

#ifdef _WIN32
		using convert_type = std::codecvt_utf8<wchar_t>;
		std::wstring_convert<convert_type, wchar_t> converter;
		std::string str = converter.to_bytes(formatted);
#else
		std::string str = formatted;
#endif

		if (!callback(static_cast<Level>(severity), str))
			std::cout << plog::severityToString(severity) << " " << str << std::endl;
	}
};

void InitLogger(Level level, LoggingCallback callback) {
    static std::unique_ptr<LogAppender> appender;
    const auto severity = static_cast<plog::Severity>(level);
	if (appender) {
		appender->callback = std::move(callback);
		InitLogger(severity, nullptr); // change the severity
	} else if (callback) {
		appender = std::make_unique<LogAppender>();
		appender->callback = std::move(callback);
		InitLogger(severity, appender.get());
	} else {
		InitLogger(severity, nullptr); // log to cout
	}
}

void InitLogger(plog::Severity severity, plog::IAppender *appender) {
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
	static plog::Logger<0> *logger = nullptr;
	static std::mutex mutex;
	std::lock_guard lock(mutex);
	if (!logger) {
		logger = &plog::init(severity, appender ? appender : &consoleAppender);
		PLOG_DEBUG << "Logger initialized";
	} else {
		logger->setMaxSeverity(severity);
		if (appender)
			logger->addAppender(appender);
	}
}

}
}