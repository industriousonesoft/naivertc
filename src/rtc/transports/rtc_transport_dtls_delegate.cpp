#include "rtc/transports/rtc_transport.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"

#include <plog/Log.h>

namespace naivertc {
    
void RtcTransport::InitDtlsTransport() {
    RTC_RUN_ON(task_queue_);
    if (dtls_transport_) {
        return;
    }
    assert(ice_transport_ && "No underlying ICE transport for DTLS transport");

    PLOG_VERBOSE << "Init DTLS transport";
    
    auto dtls_config = DtlsTransport::Configuration(certificate_, config_.mtu);
 
    // DTLS-SRTP
    if (auto local_sdp = local_sdp_; local_sdp && (local_sdp->HasAudio() || local_sdp->HasVideo())) {
        auto dtls_srtp_transport = std::make_unique<DtlsSrtpTransport>(std::move(dtls_config), ice_transport_.get(), task_queue_);
        dtls_srtp_transport->OnReceivedRtpPacket(std::bind(&RtcTransport::OnRtpPacketReceived, this, std::placeholders::_1, std::placeholders::_2));
        dtls_transport_ = std::move(dtls_srtp_transport);
    // DTLS only
    } else {
        dtls_transport_ = std::make_unique<DtlsTransport>(std::move(dtls_config), ice_transport_.get(), task_queue_);
    }

    assert(dtls_transport_ && "Failed to init DTLS transport");

    dtls_transport_->OnStateChanged(std::bind(&RtcTransport::OnDtlsTransportStateChanged, this, std::placeholders::_1));
    dtls_transport_->OnVerify(std::bind(&RtcTransport::OnDtlsVerify, this, std::placeholders::_1));
    
    dtls_transport_->Start();
}

void RtcTransport::OnDtlsTransportStateChanged(DtlsTransport::State transport_state) {

}

bool RtcTransport::OnDtlsVerify(std::string_view fingerprint) {

}

void RtcTransport::OnRtpPacketReceived(CopyOnWriteBuffer in_packet, bool is_rtcp) {

}

} // namespace naivertc