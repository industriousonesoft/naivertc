#if defined(USE_MBEDTLS)
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <plog/Log.h>

namespace naivertc {

std::mutex DtlsTransport::global_mutex_;

void DtlsTransport::Init() {
    PLOG_VERBOSE << "DTLS init";
    std::lock_guard lock(global_mutex_);
}

void DtlsTransport::Cleanup() {
    PLOG_VERBOSE << "SCTP cleanup";
    // Nothing to do
}

void DtlsTransport::InitDTLS(const Configuration& config) {

}

void DtlsTransport::DeinitDTLS() {
    
}

void DtlsTransport::InitHandshake() {

}

bool DtlsTransport::TryToHandshake() {
    return false;
}

bool DtlsTransport::IsHandshakeTimeout() {
    return false;
}

void DtlsTransport::DtlsHandshakeDone() {
    RTC_RUN_ON(&sequence_checker_);
    // Dummy
}

bool DtlsTransport::ExportKeyingMaterial(unsigned char *out, size_t olen,
                                         const char *label, size_t llen,
                                         const unsigned char *context,
                                         size_t contextlen, bool use_context) {
    return false;
}

} // namespace naivertc

#endif