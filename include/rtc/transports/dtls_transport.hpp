#ifndef _RTC_TRANSPORTS_DTLS_TRANSPORT_H_
#define _RTC_TRANSPORTS_DTLS_TRANSPORT_H_

#include "base/defines.hpp"
#include "base/certificate.hpp"
#include "base/tls.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/transports/ice_transport.hpp"

#include <optional>
#include <mutex>
#include <functional>

namespace naivertc {

using openssl_bool = int;
static const openssl_bool openssl_true = 1;
static const openssl_bool openssl_false = 0;

class RTC_CPP_EXPORT DtlsTransport : public Transport {
public:
    struct Configuration {
        std::shared_ptr<Certificate> certificate;
        std::optional<size_t> mtu;
    };
public:
    static void Init();
    static void Cleanup();
public:
    DtlsTransport(Configuration config, std::weak_ptr<IceTransport> lower, std::shared_ptr<TaskQueue> task_queue = nullptr);
    virtual ~DtlsTransport();

    bool is_client() const;

    using VerifyCallback = std::function<bool(std::string_view fingerprint)>;
    void OnVerify(VerifyCallback callback);

    virtual bool Start() override;
    virtual bool Stop() override;

    virtual int Send(Packet packet) override;

protected:
    void InitOpenSSL(const Configuration& config);
    void DeinitOpenSSL();

    void InitHandshake();
    bool TryToHandshake();
    bool IsHandshakeTimeout();
    virtual void DtlsHandshakeDone();

    bool ExportKeyingMaterial(unsigned char *out, size_t olen,
                                const char *label, size_t llen,
                                const unsigned char *context,
                                size_t contextlen, bool use_context);

    static openssl_bool CertificateCallback(int preverify_ok, X509_STORE_CTX* ctx);
    static void InfoCallback(const SSL* ssl, int where, int ret);

    static openssl_bool BioMethodNew(BIO* bio);
    static openssl_bool BioMethodFree(BIO* bio);
    static int BioMethodWrite(BIO* bio, const char* in_data, int in_size);
    static long BioMethodCtrl(BIO* bio, int cmd, long num, void* ptr);

    bool HandleVerify(std::string_view fingerprint);
    bool IsClient() const;

protected:
    virtual void Incoming(Packet in_packet) override;
    int Outgoing(Packet out_packet) override;

    int HandleDtlsWrite(const char* in_data, int in_size);

private:
    const Configuration config_;
    const bool is_client_;
    VerifyCallback verify_callback_ = nullptr;

    unsigned int curr_dscp_;

    SSL_CTX* ctx_ = NULL;
    SSL* ssl_ = NULL;
    BIO* in_bio_ = NULL;
    BIO* out_bio_ = NULL;

    // 全局变量声明式
    static BIO_METHOD* bio_methods_;
    static int transport_ex_index_;
    static std::mutex global_mutex_;

    static constexpr size_t DEFAULT_SSL_BUFFER_SIZE = 4096;
};

}

#endif