#include "base/init.hpp"
#include "common/logger.hpp"
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"
#include "rtc/transports/sctp_transport.hpp"
#include "rtc/transports/sctp_transport_usr_sctp_settings.hpp"

namespace naivertc {

void InitLogger(LoggingLevel level);

void Init(LoggingLevel level) {

    InitLogger(level);

    DtlsTransport::Init();
    DtlsSrtpTransport::Init();
    SctpTransport::Init();
    // TODO: Add public APIs for user customizing sctp
    auto sctp_settings = SctpCustomizedSettings();
    SctpTransport::CustomizeSctp(sctp_settings);
}

void Cleanup() {
    SctpTransport::Cleanup();
    DtlsSrtpTransport::Cleanup();
    DtlsTransport::Cleanup();
}

void InitLogger(LoggingLevel level) {
    logging::Level plog_level = logging::Level::NONE;
    switch (level) {
    case LoggingLevel::NONE:
        plog_level = logging::Level::NONE;
        break;
    case LoggingLevel::DEBUG:
        plog_level = logging::Level::DEBUG;
        break;
    case LoggingLevel::WARNING:
        plog_level = logging::Level::WARNING;
        break;
    case LoggingLevel::INFO:
        plog_level = logging::Level::INFO;
        break;
    case LoggingLevel::ERROR:
        plog_level = logging::Level::ERROR;
        break;
    case LoggingLevel::VERBOSE:
        plog_level = logging::Level::VERBOSE;
        break;
    default:
        break;
    }
    // TODO: add a logging callback for serialize log
    logging::InitLogger(plog_level);
}
    
} // namespace naivertc
