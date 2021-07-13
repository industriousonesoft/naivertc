#include "base/init.hpp"
#include "pc/transports/dtls_transport.hpp"
#include "pc/transports/dtls_srtp_transport.hpp"
#include "pc/transports/sctp_transport.hpp"
#include "pc/transports/sctp_transport_usr_sctp_settings.hpp"

namespace naivertc {

void Init() {
    DtlsTransport::Init();
    DtlsSrtpTransport::Init();
    SctpTransport::Init();
    // TODO: Add public APIs for user customizing sctp
    auto sctp_settings = SctpCustomizedSettings();
    SctpTransport::CustomizeSctp(std::move(sctp_settings));
}

void Cleanup() {
    SctpTransport::Cleanup();
    DtlsSrtpTransport::Cleanup();
    DtlsTransport::Cleanup();
}
    
} // namespace naivertc
