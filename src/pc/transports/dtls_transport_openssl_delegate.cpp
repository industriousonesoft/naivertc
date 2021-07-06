#include "pc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"
#include "base/packet.hpp"
#include "base/internals.hpp"

#include <plog/Log.h>

#include <sys/time.h>
#include <chrono>

namespace naivertc {

// 静态变量定义式
BIO_METHOD* DtlsTransport::bio_methods_ = NULL;
int DtlsTransport::transport_ex_index_ = -1;
std::mutex DtlsTransport::global_mutex_;

// Static methods
void DtlsTransport::Init() {
    std::lock_guard lock(global_mutex_);

    openssl::init();

    if (!bio_methods_) {
        bio_methods_ = BIO_meth_new(BIO_TYPE_BIO, "DTLS writer");
        if (!bio_methods_) {
            throw std::runtime_error("Failed to create BIO methods for DTLS writer.");
        }
        BIO_meth_set_create(bio_methods_, BioMethodNew);
        BIO_meth_set_destroy(bio_methods_, BioMethodFree);
        BIO_meth_set_write(bio_methods_, BioMethodWrite);
        BIO_meth_set_ctrl(bio_methods_, BioMethodCtrl);
    }
    if (transport_ex_index_ < 0) {
        transport_ex_index_ = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    }
}

void DtlsTransport::Cleanup() {
    // Nothing to do
}

// Init methods
void DtlsTransport::InitOpenSSL(const Config& config) {
    PLOG_DEBUG << "Initializing DTLS transport (OpenSSL)";

    if (!config.certificate) {
        throw std::invalid_argument("DTLS certificate is null.");
    }

    try {
        ctx_ = SSL_CTX_new(DTLS_method());

        if (!ctx_) {
            throw std::runtime_error("Failed to create SSL context for DTLS.");
        }

        // RFC 8261: SCTP performs segmentation and reassembly based on the path MTU.
		// Therefore, the DTLS layer MUST NOT use any compression algorithm.
		// See https://tools.ietf.org/html/rfc8261#section-5
		// RFC 8827: Implementations MUST NOT implement DTLS renegotiation
		// See https://tools.ietf.org/html/rfc8827#section-6.5
		SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION | SSL_OP_NO_QUERY_MTU |
		                              SSL_OP_NO_RENEGOTIATION);
        // DTLS version 1
        SSL_CTX_set_min_proto_version(ctx_, DTLS1_VERSION);
        // Set whether we should read as many input bytes as possible (for non-blocking reads) or not
        SSL_CTX_set_read_ahead(ctx_, openssl_true);
        SSL_CTX_set_quiet_shutdown(ctx_, openssl_true);
        SSL_CTX_set_info_callback(ctx_, InfoCallback);

        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, CertificateCallback);
        SSL_CTX_set_verify_depth(ctx_, 1);

        openssl::check(SSL_CTX_set_cipher_list(ctx_, "ALL:!LOW:!EXP:!RC4:!MD5:@STRENGTH"), "Failed to set SSL priorities.");

        auto [x509, pkey] = config.certificate->credentials();
        SSL_CTX_use_certificate(ctx_, x509);
        SSL_CTX_use_PrivateKey(ctx_, pkey);

        openssl::check(SSL_CTX_check_private_key(ctx_), "SSL local private key check failed.");

        ssl_ = SSL_new(ctx_);
        if (!ssl_) {
            throw std::runtime_error("Failed to create SSL instance.");
        }

        // pass this pointer to the callback
        SSL_set_ex_data(ssl_, transport_ex_index_, this);

        if (is_client_) {
            SSL_set_connect_state(ssl_);
        }else {
            SSL_set_accept_state(ssl_);
        }

        // BIO_s_mem是一个封装了内存操作的BIO接口， 包括对内存的读写操作
        in_bio_ = BIO_new(BIO_s_mem());
        out_bio_ = BIO_new(bio_methods_);
        if (!in_bio_ || !out_bio_) {
            throw std::runtime_error("Failed to create BIO.");
        }

        BIO_set_mem_eof_return(in_bio_, BIO_EOF);
        BIO_set_data(out_bio_, this);
        // in_bio->ssl->out_bio
        SSL_set_bio(ssl_, in_bio_, out_bio_);

        auto ecdh = std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)>(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), EC_KEY_free);
        SSL_set_options(ssl_, SSL_OP_SINGLE_ECDH_USE);
        SSL_set_tmp_ecdh(ssl_, ecdh.get());

        // RFC 8827: The DTLS-SRTP protection profile SRTP_AES128_CM_HMAC_SHA1_80 MUST be supported
		// See https://tools.ietf.org/html/rfc8827#section-6.5 Warning:
		// SSL_set_tlsext_use_srtp() returns 0 on success and 1 on error
        if (SSL_set_tlsext_use_srtp(ssl_, "SRTP_AES128_CM_SHA1_80")) {
            throw std::runtime_error("Failed to set SRTP profile: " + openssl::error_string(ERR_get_error()));
        }

    }catch (const std::exception& exp) {
        DeinitOpenSSL();
        throw exp.what();
    }
}

void DtlsTransport::DeinitOpenSSL() {
    if (ssl_) {
        SSL_free(ssl_);
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
    }
}

void DtlsTransport::InitHandshake() {
    if (!ssl_) {
        throw std::runtime_error("SSL instance is not created yet.");
    }
    // 握手成功之前的MTU值
    size_t mtu = config_.mtu.value_or(DEFAULT_MTU_SIZE) - 8 - 40; // UDP/IPv6
    SSL_set_mtu(ssl_, static_cast<unsigned int>(mtu));

    PLOG_VERBOSE << "SSL MTU set to " << mtu;

    int ret = SSL_do_handshake(ssl_);
    openssl::check(ssl_, ret, "Initiate handshake failed.");
}

bool DtlsTransport::TryToHandshake() {
    if (!ssl_) {
        throw std::runtime_error("SSL instance is not created yet.");
    }
    int ret = SSL_do_handshake(ssl_);
    if (!openssl::check(ssl_, ret, "Continue to handshake failed.")) {
        return false;
    }

    if (SSL_is_init_finished(ssl_)) {
        // RFC 8261: DTLS MUST support sending messages larger than the current path
        // MTU See https://tools.ietf.org/html/rfc8261#section-5
        SSL_set_mtu(ssl_, DEFAULT_SSL_BUFFER_SIZE + 1 /* buffer eof byte? */);

        PLOG_INFO << "DTLS handshake finished.";
        return true;
    }else {
        return false;
    }
}

bool DtlsTransport::IsHandshakeTimeout() {
    if (!ssl_) {
        throw std::runtime_error("SSL instance is not created yet.");
    }
    // DTLSv1_handle_timeout is called when a DTLS handshake timeout expires. If no timeout had expired, 
    // it returns 0. Otherwise, it retransmits the previous flight of handshake messages and returns 1. 
    // If too many timeouts had expired without progress or an error occurs, it returns -1.
    int ret = DTLSv1_handle_timeout(ssl_);
    if (ret < 0) {
        return true;
    }else if (ret > 0) {
        LOG_VERBOSE << "Openssl did DTLS retransmit";
    }

    struct timeval timeout = {};
    // DTLSv1_get_timeout queries the next DTLS handshake timeout.
    // If there is a timeout in progress, it sets *out to the time remaining and returns one. 
    // Otherwise, it returns zero.
    if (DTLSv1_get_timeout(ssl_, &timeout)) {
        auto duration_ms = std::chrono::milliseconds(timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
        // Also handle hand shake manually because OpenSSL actually doesn't...
        // OpenSSL backs off exponentially in base 2 starting from the recommended 1s,
        // so this allow for 5 retansmissions and faild after roughly 30s.
        if (duration_ms > std::chrono::milliseconds(30000)) {
            return true;
        }else {
            LOG_VERBOSE << "OpenSSL DTLS retransmit timeout is " << duration_ms.count() << "ms";
        }
    }
    return false;
}

void DtlsTransport::HandshakeDone() {
    // Dummy
}

// Callback methods
openssl_bool DtlsTransport::CertificateCallback(int preverify_ok, X509_STORE_CTX* ctx) {
    SSL* ssl = static_cast<SSL *>(X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    DtlsTransport* transport = static_cast<DtlsTransport*>(SSL_get_ex_data(ssl, DtlsTransport::transport_ex_index_));
    // Detect the tansport pointer is still available
    if (WeakPtrManager::SharedInstance()->TryLock(transport)) {
        X509* crt = X509_STORE_CTX_get_current_cert(ctx);
        std::string fingerprint = Certificate::MakeFingerprint(crt);
        return transport->HandleVerify(fingerprint) ? openssl_true /* true */ : openssl_false /* false */;
    }else {
        return openssl_false;
    }
}

void DtlsTransport::InfoCallback(const SSL* ssl, int where, int ret) {
    // SSL_CB_LOOP                     0x01
    // SSL_CB_EXIT                     0x02
    // SL_CB_READ                      0x04
    // SSL_CB_WRITE                    0x08
    // SSL_CB_ALERT                    0x4000/* used in callback */
    // SSL_CB_READ_ALERT               (SSL_CB_ALERT|SSL_CB_READ)
    // SSL_CB_WRITE_ALERT              (SSL_CB_ALERT|SSL_CB_WRITE)
    // SSL_CB_ACCEPT_LOOP              (SSL_ST_ACCEPT|SSL_CB_LOOP)
    // SSL_CB_ACCEPT_EXIT              (SSL_ST_ACCEPT|SSL_CB_EXIT)
    // SSL_CB_CONNECT_LOOP             (SSL_ST_CONNECT|SSL_CB_LOOP)
    // SSL_CB_CONNECT_EXIT             (SSL_ST_CONNECT|SSL_CB_EXIT)
    // SSL_CB_HANDSHAKE_START          0x10
    // SSL_CB_HANDSHAKE_DONE           0x20

    // DtlsTransport* transport = static_cast<DtlsTransport*>(SSL_get_ex_data(ssl, DtlsTransport::transport_ex_index_));

    // Callback has been called to indicate state change inside a loop.
    if (where & SSL_CB_LOOP) {
        PLOG_INFO << "Loop state changed: " << SSL_alert_desc_string_long(ret);
    // Callback has been called to indicate error exit of a handshake function.
    }else if (where & SSL_CB_EXIT) {
        PLOG_INFO << "Exit error: " << SSL_alert_desc_string_long(ret);
    // Callback has been called during read operation.
    }else if (where & SSL_CB_READ) {
        
    // Callback has been called during write operation.
    }else if (where & SSL_CB_WRITE) {
        
    }else if (where & SSL_CB_READ_ALERT) {
        PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
    }else if (where & SSL_CB_WRITE_ALERT) {
        PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
    }else if (where & SSL_CB_ACCEPT_LOOP) {
        PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
    }else if (where & SSL_CB_ACCEPT_EXIT) {
        PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
    }else if (where & SSL_CB_CONNECT_LOOP) {
        PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
    }else if (where & SSL_CB_CONNECT_EXIT) {
        PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
    // Callback has been called because a new handshake is started.
    }else if (where & SSL_CB_HANDSHAKE_START) {
        PLOG_INFO << "Handshake start";
    // Callback has been called because a handshake is finished.
    }else if (where & SSL_CB_HANDSHAKE_DONE) {
        PLOG_INFO << "handshake done";
    }else {
        if (where & SSL_CB_ALERT) {
            if (ret != 256) {
                PLOG_ERROR << "DTLS alert: " << SSL_alert_desc_string_long(ret);
            }
            // TODO: close connection
        } 
    }
}

openssl_bool DtlsTransport::BioMethodNew(BIO* bio) {
    BIO_set_init(bio, openssl_true);
    BIO_set_data(bio, NULL);
    BIO_set_shutdown(bio, openssl_false);
    return openssl_true;
}

openssl_bool DtlsTransport::BioMethodFree(BIO* bio) {
    if (!bio) {
        return openssl_false;
    }else {
        BIO_set_data(bio, NULL);
        return openssl_true;
    }
}

int DtlsTransport::BioMethodWrite(BIO* bio, const char* in, int in_size) {
    if (in_size <= 0) {
        return in_size;
    }
    auto transport = reinterpret_cast<DtlsTransport*>(BIO_get_data(bio));
    if (WeakPtrManager::SharedInstance()->TryLock(transport)) {
        auto bytes = reinterpret_cast<const std::byte*>(in);
        auto pakcet = Packet::Create(std::move(bytes), in_size);
        transport->Outgoing(pakcet);
        return in_size;
    }
    return -1;
}

long DtlsTransport::BioMethodCtrl(BIO* bio, int cmd, long num, void* ptr) {
    switch (cmd) {
    case BIO_CTRL_FLUSH:
		return 1;
	case BIO_CTRL_DGRAM_QUERY_MTU:
		return 0; // SSL_OP_NO_QUERY_MTU must be set
	case BIO_CTRL_WPENDING:
	case BIO_CTRL_PENDING:
		return 0;
	default:
		break;
	}
	return 0;
}

}