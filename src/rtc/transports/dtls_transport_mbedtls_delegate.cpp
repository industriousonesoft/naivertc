#if defined(USE_MBEDTLS)
#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"
#include "rtc/transports/ice_transport.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
// Personalization string.
constexpr char pers_server[] = "dtls_server";
constexpr char pers_client[] = "dtls_client";

// mbedtls_debug
static void mbedtls_debug(void *ctx, int level,
                          const char *file, int line,
                          const char *str) {
    PLOG_DEBUG << file << ":" << line << ":" << str;
}

// READ_TIMEOUT_MS
#define READ_TIMEOUT_MS 10000   /* 10 seconds */

// RFC 8827: The DTLS-SRTP protection profile SRTP_AES128_CM_HMAC_SHA1_80 MUST be supported
// See https://tools.ietf.org/html/rfc8827#section-6.5
const mbedtls_ssl_srtp_profile default_dtls_srtp_profiles[] = {
        MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
        // MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_32,
        // MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_80,
        // MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_32,
        // MBEDTLS_TLS_SRTP_UNSET
};

/* Supported SRTP mode needs a maximum of :
 * - 16 bytes for key (AES-128)
 * - 14 bytes SALT
 * One for sender, one for receiver context
 */
#define MBEDTLS_TLS_SRTP_MAX_KEY_MATERIAL_LENGTH    60

typedef struct dtls_srtp_keys
{
    unsigned char master_secret[48];
    unsigned char randbytes[64];
    mbedtls_tls_prf_types tls_prf_type;
} dtls_srtp_keys;
dtls_srtp_keys dtls_srtp_keying;

void dtls_srtp_key_derivation( void *p_expkey,
                               mbedtls_ssl_key_export_type secret_type,
                               const unsigned char *secret,
                               size_t secret_len,
                               const unsigned char client_random[32],
                               const unsigned char server_random[32],
                               mbedtls_tls_prf_types tls_prf_type )
{
    dtls_srtp_keys *keys = (dtls_srtp_keys *)p_expkey;

    /* We're only interested in the TLS 1.2 master secret */
    if( secret_type != MBEDTLS_SSL_KEY_EXPORT_TLS12_MASTER_SECRET )
        return;
    if( secret_len != sizeof( keys->master_secret ) )
        return;

    memcpy( keys->master_secret, secret, sizeof( keys->master_secret ) );
    memcpy( keys->randbytes, client_random, 32 );
    memcpy( keys->randbytes + 32, server_random, 32 );
    keys->tls_prf_type = tls_prf_type;
}

} // namespace

void DtlsTransport::Init() {
    PLOG_VERBOSE << "DTLS init";
    // Nothing to do
}

void DtlsTransport::Cleanup() {
    PLOG_VERBOSE << "DTLS cleanup";
    // Nothing to do
}

void DtlsTransport::InitDTLS(const Configuration& config) {
    RTC_RUN_ON(&sequence_checker_);
    PLOG_DEBUG << "Initializing DTLS transport (MbedTLS) as a " << (is_client_ ? "server" : "client");
    try {
        mbedtls_ssl_init(&ssl_);
        mbedtls_ssl_config_init(&ssl_conf_);
        // mbedtls_ssl_cookie_init(&cookie_);
        mbedtls_entropy_init(&entropy_);
        mbedtls_ctr_drbg_init(&ctr_drbg_);
        mbedtls_x509_crt_init(&cert_);
        mbedtls_pk_init(&pkey_);

        // Debug levels - 0 No debug - 1 Error - 2 State change - 3 Informational - 4 Verbose
        mbedtls_debug_set_threshold(1);

        // Seed the RNG (random number generator)
        PLOG_VERBOSE << "Seeding the random number generator...";
        auto pers = is_client_ ? pers_client : pers_server;
        int ret = mbedtls_ctr_drbg_seed(&ctr_drbg_, 
                                         mbedtls_entropy_func, 
                                         &entropy_, 
                                         (const unsigned char*)pers,
                                         strlen(pers));
        if (ret != 0) {
            throw std::runtime_error("Failed to seed the RNG.");
        }

        auto [crt_pem, pkey_pem] = config.certificate->GetCredentialsInPEM();

        // Load the certificates and private RSA key.
        ret = mbedtls_x509_crt_parse(&cert_, (const unsigned char*)crt_pem.data(), crt_pem.size());
        if (ret != 0) {
            throw std::runtime_error("Failed to parse x509 with certificates in PEM formate.");
        }

        ret = mbedtls_pk_parse_key(&pkey_, (const unsigned char*)pkey_pem.data(), pkey_pem.size(), NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg_);
        if (ret != 0) {
            throw std::runtime_error("Failed to parse private key of ECDSA.");
        }

        // Config DTLS
        ret = mbedtls_ssl_config_defaults(&ssl_conf_, 
                                          (is_client_ ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER),
                                          MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);

        if (ret != 0) {
            throw std::runtime_error("Failed to config DTLS.");
        }

        // TODO: Verify fingerprint
        mbedtls_ssl_conf_authmode(&ssl_conf_, MBEDTLS_SSL_VERIFY_OPTIONAL);
        mbedtls_ssl_conf_rng(&ssl_conf_, mbedtls_ctr_drbg_random, &ctr_drbg_);
        mbedtls_ssl_conf_dbg(&ssl_conf_, mbedtls_debug, nullptr);
        mbedtls_ssl_conf_read_timeout(&ssl_conf_, READ_TIMEOUT_MS);
        // DTLS-SRTP
        mbedtls_ssl_conf_dtls_srtp_protection_profiles(&ssl_conf_, default_dtls_srtp_profiles);

        mbedtls_ssl_conf_ca_chain(&ssl_conf_, &cert_, nullptr);
        ret = mbedtls_ssl_conf_own_cert(&ssl_conf_, &cert_, &pkey_);
        if (ret != 0) {
            throw std::runtime_error("Faild to verify server cert and private key.");
        }

        // cookie only needed on server.
        if (!is_client_) {
            // ret = mbedtls_ssl_cookie_setup(&cookie_, mbedtls_ctr_drbg_random, &ctr_drbg_);
            // if (ret != 0) {
            //     throw std::runtime_error("Failed to set DTLS cookie.");
            // }
            // FIXME: SSl cookie check failed as no cookie in the client hello message.
            mbedtls_ssl_conf_dtls_cookies(&ssl_conf_, mbedtls_ssl_cookie_write, nullptr, &cookie_);
        }
    
        // setup SSL configs.
        ret = mbedtls_ssl_setup(&ssl_, &ssl_conf_);
        if (ret != 0) {
            throw std::runtime_error("Failed to setup DTLS.");
        }

        /* For HelloVerifyRequest cookies, server only, DTLS only. */
        // if (!is_client_) {
        //     auto client_ip = static_cast<IceTransport*>(lower_)->GetRemoteAddress();
        //     ret = mbedtls_ssl_set_client_transport_id(&ssl_, (const unsigned char *)client_ip->data(), client_ip->size());
        //     if (ret != 0) {
        //         throw std::runtime_error("Failed to set clinet transport id.");
        //     }
        // }

        // BIO callbacks
        mbedtls_ssl_set_bio(&ssl_, this, mbedtls_custom_send, mbedtls_custom_recv, nullptr);

        // Timer
        mbedtls_ssl_set_timer_cb(&ssl_, &timer_, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
        
        // Key export callback
        mbedtls_ssl_set_export_keys_cb(&ssl_, dtls_srtp_key_derivation,
                                        &dtls_srtp_keying);

    } catch (const std::exception& exp) {
        DeinitDTLS();
        PLOG_ERROR << "Failed to init DTLS transport (MbedTLS)" << exp.what();
    }

}

void DtlsTransport::DeinitDTLS() {
    RTC_RUN_ON(&sequence_checker_);
    mbedtls_x509_crt_free( &cert_ );
    mbedtls_pk_free( &pkey_ );
    mbedtls_ssl_free( &ssl_ );
    mbedtls_ssl_config_free( &ssl_conf_ );
    // mbedtls_ssl_cookie_free( &cookie_ );
    mbedtls_ctr_drbg_free( &ctr_drbg_ );
    mbedtls_entropy_free( &entropy_ );
}

void DtlsTransport::InitHandshake() {
    RTC_RUN_ON(&sequence_checker_);
  
    // 握手成功之前的MTU值
    size_t mtu = config_.mtu.value_or(kDefaultMtuSize) - 8 - 40; // UDP/IPv6
    mbedtls_ssl_set_mtu(&ssl_, static_cast<uint16_t>(mtu));

    PLOG_VERBOSE << "SSL MTU set to " << mtu;

    int ret = mbedtls_ssl_handshake(&ssl_);
    if(ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
        PLOG_WARNING << "Hello verification requested.";
    }
    // Non-blocking.
    if (ret != 0) {
        PLOG_WARNING << "Ready to init handshake";
    }
}

bool DtlsTransport::TryToHandshake() {

    // 1 if handshake is over, 0 if it is still ongoing.
    int ret = mbedtls_ssl_handshake(&ssl_);
    if(ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
        PLOG_WARNING << "Hello verification requested.";
        return false;
    }
    if (ret != 0) {
        PLOG_WARNING << "Still woking on handshake...";
        return false;
    }

    // RFC 8261: DTLS MUST support sending messages larger than the current path
    // MTU See https://tools.ietf.org/html/rfc8261#section-5
    mbedtls_ssl_set_mtu(&ssl_, DEFAULT_SSL_BUFFER_SIZE + 1 /* buffer eof byte? */);
    DtlsHandshakeDone();

    PLOG_INFO << "DTLS handshake finished.";

    return true;
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
    assert(olen == MBEDTLS_TLS_SRTP_MAX_KEY_MATERIAL_LENGTH);
    int ret = mbedtls_ssl_tls_prf(dtls_srtp_keying.tls_prf_type,
                                  dtls_srtp_keying.master_secret,
                                  sizeof( dtls_srtp_keying.master_secret ),
                                  label,
                                  dtls_srtp_keying.randbytes,
                                  sizeof( dtls_srtp_keying.randbytes ),
                                  out,
                                  olen);

    if(ret != 0) {
        PLOG_WARNING << "Failed to export keying material, ret=" << ret;
        return false;
    } else {
        return true;
    }   
}

int DtlsTransport::mbedtls_custom_send(void *ctx, const unsigned char *buf, size_t len) {
    auto transport = reinterpret_cast<DtlsTransport*>(ctx);
    if (WeakPtrManager::SharedInstance()->Lock(transport)) {
        auto bytes = reinterpret_cast<const uint8_t*>(buf);
        int write_size = transport->OnDtlsWrite(CopyOnWriteBuffer(bytes, len));
        PLOG_VERBOSE << "Send DTLS size: " << len << " : " << write_size;
        return len;
    }
    return -1;
}

int DtlsTransport::mbedtls_custom_recv(void *ctx, unsigned char *buf, size_t len) {
    auto transport = reinterpret_cast<DtlsTransport*>(ctx);
    if (WeakPtrManager::SharedInstance()->Lock(transport)) {
        if (transport->curr_in_packet_) {
            size_t write_size = std::min(transport->curr_in_packet_->size(), len);
            memcpy(buf, transport->curr_in_packet_->cdata(), write_size);
            PLOG_VERBOSE << "DTLS write size: " << write_size;
            transport->curr_in_packet_.reset();
            return write_size;
        } else {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
    }
    return -1;
}

} // namespace naivertc

#endif