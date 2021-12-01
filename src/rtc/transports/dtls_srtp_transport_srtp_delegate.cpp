#include "rtc/transports/dtls_srtp_transport.hpp"

#include <plog/Log.h>

#include <string>

namespace {
    /*
    The exporter label for this usage is "EXTRACTOR-dtls_srtp".  (The
    "EXTRACTOR" prefix is for historical compatibility.)
    RFC 5764 4.2.  Key Derivation
    */
    static const std::string kDtlsSrtpExporterLabel = "EXTRACTOR-dtls_srtp";
}

namespace naivertc {

void DtlsSrtpTransport::Init() {
    PLOG_VERBOSE << "SRTP init";
    srtp_init();
}

void DtlsSrtpTransport::Cleanup() {
    PLOG_VERBOSE << "SRTP cleanup";
    srtp_shutdown();
}

void DtlsSrtpTransport::CreateSrtp() {
    RTC_RUN_ON(task_queue_);
    if (srtp_err_status_t err = srtp_create(&srtp_in_, nullptr)) {
		throw std::runtime_error("SRTP create failed, status=" + std::to_string(static_cast<int>(err)));
	}
	if (srtp_err_status_t err = srtp_create(&srtp_out_, nullptr)) {
		srtp_dealloc(srtp_in_);
		throw std::runtime_error("SRTP create failed, status=" + std::to_string(static_cast<int>(err)));
	}
}

void DtlsSrtpTransport::DestroySrtp() {
    RTC_RUN_ON(task_queue_);
    srtp_dealloc(srtp_in_);
    srtp_dealloc(srtp_out_);
}

void DtlsSrtpTransport::InitSrtp() {
    RTC_RUN_ON(task_queue_);
    static_assert(SRTP_AES_ICM_128_KEY_LEN_WSALT == SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN);

    const size_t material_len = SRTP_AES_ICM_128_KEY_LEN_WSALT * 2;
    unsigned char material[material_len];
     
    PLOG_INFO << "Deriving SRTP keying material (OpenSSL)";

    if (DtlsTransport::ExportKeyingMaterial(material, material_len, kDtlsSrtpExporterLabel.c_str(), kDtlsSrtpExporterLabel.size(), nullptr, 0, false) == false) {
        throw std::runtime_error("Failed to derive SRTP key.");
    }
    
    // client_key|server_key|client_salt|server_salt
    size_t offset = 0;
    memcpy(&client_write_key_[0], &material[offset], SRTP_AES_128_KEY_LEN);
    offset += SRTP_AES_128_KEY_LEN;
    memcpy(&server_write_key_[0], &material[offset], SRTP_AES_128_KEY_LEN);
    offset += SRTP_AES_128_KEY_LEN;
    memcpy(&client_write_key_[SRTP_AES_128_KEY_LEN], &material[offset], SRTP_SALT_LEN);
    offset += SRTP_SALT_LEN;
    memcpy(&server_write_key_[SRTP_AES_128_KEY_LEN], &material[offset], SRTP_SALT_LEN);

    srtp_policy_t inbound = {};
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&inbound.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&inbound.rtcp);
    inbound.ssrc.type = ssrc_any_inbound;
    inbound.key = is_client() ? server_write_key_ : client_write_key_;
    inbound.window_size = 1024;
    inbound.allow_repeat_tx = true;
    inbound.next = nullptr;

    if (srtp_err_status_t err = srtp_add_stream(srtp_in_, &inbound)) {
        throw std::runtime_error("Failed to add SRTP inbound stream, status: " + std::to_string(static_cast<int>(err)));
    }

    srtp_policy_t outbound = {};
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&outbound.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&outbound.rtcp);
    outbound.ssrc.type = ssrc_any_outbound;
    outbound.key = is_client() ? client_write_key_ : server_write_key_;
    outbound.window_size = 1024;
    outbound.allow_repeat_tx = true;
    outbound.next = nullptr;

    if (srtp_err_status_t err = srtp_add_stream(srtp_out_, &outbound)) {
        throw std::runtime_error("Failed to add SRTP outbound stream, status: " + std::to_string(static_cast<int>(err)));
    }
}

} // namespace naivertc 


