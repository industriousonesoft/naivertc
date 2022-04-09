#if defined(USE_MBEDTLS)
#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

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
    PLOG_DEBUG << "Initializing DTLS transport (MbedTLS)";
    try {
        mbedtls_ssl_init(&ssl_);
        mbedtls_ssl_config_init(&ssl_conf_);
        mbedtls_ssl_cookie_init(&cookie_);
        mbedtls_entropy_init(&entropy_);
        mbedtls_ctr_drbg_init(&ctr_drbg_);
        mbedtls_x509_crt_init(&cert_);
        mbedtls_pk_init(&pkey_);

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
            throw std::runtime_error("Failed to parse x509 with CA certificates in PEM formate.");
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

        mbedtls_ssl_conf_ca_chain(&ssl_conf_, &cert_, nullptr);
        ret = mbedtls_ssl_conf_own_cert(&ssl_conf_, &cert_, &pkey_);
        if (ret != 0) {
            throw std::runtime_error("Faild to verify server cert and private key.");
        }

        // cookie
        ret = mbedtls_ssl_cookie_setup(&cookie_, mbedtls_ctr_drbg_random, &ctr_drbg_);
        if (ret != 0) {
            throw std::runtime_error("Failed to set DTLS cookie.");
        }
        mbedtls_ssl_conf_dtls_cookies(&ssl_conf_, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check, &cookie_);

        // setup
        ret = mbedtls_ssl_setup(&ssl_, &ssl_conf_);
        if (ret != 0) {
            throw std::runtime_error("Failed to setup DTSL");
        }

        mbedtls_ssl_set_bio(&ssl_, this, MbedtlsNetSend, nullptr, nullptr);
        // Timer
        mbedtls_ssl_set_timer_cb(&ssl_, &timer_, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

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
    mbedtls_ssl_cookie_free( &cookie_ );
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
    if (ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
        PLOG_WARNING << "Hello verification requested.";
    }
    if (ret != 0) {
        PLOG_WARNING << "Init handshake failed, ret=" << ret;
    }
}

bool DtlsTransport::TryToHandshake() {
    int ret = mbedtls_ssl_handshake(&ssl_);
    if (ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
        PLOG_WARNING << "Hello verification requested.";
        return false;
    }
    if (ret != 0) {
        PLOG_WARNING << "Init handshake failed, ret=" << ret;
        return false;
    } else {

        // RFC 8261: DTLS MUST support sending messages larger than the current path
        // MTU See https://tools.ietf.org/html/rfc8261#section-5
        mbedtls_ssl_set_mtu(&ssl_, DEFAULT_SSL_BUFFER_SIZE + 1 /* buffer eof byte? */);
        DtlsHandshakeDone();

        PLOG_INFO << "DTLS handshake finished.";

        return true;
    }
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

// Callbacks
int DtlsTransport::MbedtlsNetSend(void *ctx, const unsigned char *buf, size_t len) {
    auto transport = reinterpret_cast<DtlsTransport*>(ctx);
    if (WeakPtrManager::SharedInstance()->Lock(transport)) {
        auto bytes = reinterpret_cast<const uint8_t*>(buf);
        int write_size = transport->OnDtlsWrite(CopyOnWriteBuffer(bytes, len));
        PLOG_VERBOSE << "Send DTLS size: " << len << " : " << write_size;
        return len;
    }
    return -1;
}

int DtlsTransport::MbedtlsNetRecv(void *ctx, unsigned char *buf, size_t len) {
    auto transport = reinterpret_cast<DtlsTransport*>(ctx);
    if (WeakPtrManager::SharedInstance()->Lock(transport)) {
        auto bytes = reinterpret_cast<const uint8_t*>(buf);
        
        PLOG_VERBOSE << "Recv DTLS size: " << len;
        return len;
    }
    return -1;
}

} // namespace naivertc

#endif