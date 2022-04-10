#ifndef _RTC_TRANSPORTS_DTLS_TRANSPORT_H_
#define _RTC_TRANSPORTS_DTLS_TRANSPORT_H_

#include "base/defines.hpp"
#include "base/certificate.hpp"
#include "base/tls.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/transports/base_transport.hpp"

#include <optional>
#include <functional>
#include <mutex>

namespace naivertc {

#if !defined(USE_MBEDTLS)
using openssl_bool = int;
static const openssl_bool openssl_true = 1;
static const openssl_bool openssl_false = 0;
#endif

class DtlsTransport : public BaseTransport {
public:
    struct Configuration {
        std::shared_ptr<Certificate> certificate = nullptr;
        std::optional<size_t> mtu = std::nullopt;
    };
public:
    static void Init();
    static void Cleanup();
public:
    DtlsTransport(Configuration config, bool is_client, BaseTransport* lower);
    virtual ~DtlsTransport() override;

    bool IsClient() const;

    using VerifyCallback = std::function<bool(std::string)>;
    void OnVerify(VerifyCallback callback);

    virtual bool Start() override;
    virtual bool Stop() override;

    virtual int Send(CopyOnWriteBuffer packet, PacketOptions options) override;

protected:
    virtual void Incoming(CopyOnWriteBuffer in_packet) override;
    virtual int Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) override;

    bool ExportKeyingMaterial(unsigned char *out, size_t olen,
                              const char *label, size_t llen,
                              const unsigned char *context,
                              size_t contextlen, bool use_context);

private:
    void InitDTLS(const Configuration& config);
    void DeinitDTLS();

    void InitHandshake();
    bool TryToHandshake();
    bool IsHandshakeTimeout();
    virtual void DtlsHandshakeDone();

#if defined(USE_MBEDTLS)
    void InitSSL();

    static int my_cert_verify(void *ctx, 
                              mbedtls_x509_crt *crt, 
                              int depth, uint32_t *flags);

    static int mbedtls_custom_send(void *ctx, const unsigned char *buf, size_t len);
    static int mbedtls_custom_recv(void *ctx, unsigned char *buf, size_t len);
    
    void mbedtls_bio_write(CopyOnWriteBuffer packet);
#else
    static openssl_bool CertificateCallback(int preverify_ok, X509_STORE_CTX* ctx);
    static void InfoCallback(const SSL* ssl, int where, int ret);
    static openssl_bool BioMethodNew(BIO* bio);
    static openssl_bool BioMethodFree(BIO* bio);
    static int BioMethodWrite(BIO* bio, const char* in_data, int in_size);
    static long BioMethodCtrl(BIO* bio, int cmd, long num, void* ptr);
#endif

    bool HandleVerify(std::string fingerprint);

    int OnDtlsWrite(CopyOnWriteBuffer data);
    int HandleDtlsWrite(CopyOnWriteBuffer data);

private:
    const Configuration config_;
    const bool is_client_;
    const PacketOptions handshake_packet_options_;
    std::optional<PacketOptions> user_packet_options_;

#if defined(USE_MBEDTLS)
    // mbedtls
    mbedtls_ssl_cookie_ctx cookie_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
    mbedtls_ssl_context ssl_;
    mbedtls_ssl_config ssl_conf_;
    mbedtls_timing_delay_context timer_;
    mbedtls_x509_crt cert_;
    mbedtls_pk_context pkey_;

    std::optional<CopyOnWriteBuffer> curr_in_packet_;
    bool waiting_for_reconnection = false;
#else
    // openssl
    SSL_CTX* ctx_ = NULL;
    SSL* ssl_ = NULL;
    BIO* in_bio_ = NULL;
    BIO* out_bio_ = NULL;

    // 全局变量声明式
    static BIO_METHOD* bio_methods_;
    static int transport_ex_index_;
    static std::mutex global_mutex_;
#endif

    static constexpr size_t DEFAULT_SSL_BUFFER_SIZE = 4096;
    uint8_t ssl_read_buffer_[DEFAULT_SSL_BUFFER_SIZE];

    VerifyCallback verify_callback_ = nullptr;
};

}

#endif