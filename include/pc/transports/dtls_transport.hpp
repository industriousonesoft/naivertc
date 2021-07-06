#ifndef _PC_DTLS_TRANSPORT_H_
#define _PC_DTLS_TRANSPORT_H_

#include "base/defines.hpp"
#include "base/certificate.hpp"
#include "base/tls.hpp"
#include "pc/transports/ice_transport.hpp"

#include <optional>
#include <mutex>
#include <functional>

namespace naivertc {

using openssl_bool = int;
static const openssl_bool openssl_true = 1;
static const openssl_bool openssl_false = 0;

class RTC_CPP_EXPORT DtlsTransport : public Transport {
public:
    struct Config {
        std::shared_ptr<Certificate> certificate;
        std::optional<size_t> mtu;
    };
public:
    static void Init();
    static void Cleanup();
public:
    DtlsTransport(std::shared_ptr<IceTransport> lower, const Config& config);
    ~DtlsTransport();

    bool is_client() const { return is_client_; }

    using VerifyCallback = std::function<bool(const std::string& fingerprint)>;
    void OnVerify(VerifyCallback callback);

    void Start(StartedCallback callback = nullptr) override;
    void Stop(StopedCallback callback = nullptr) override;
    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr) override;

protected:
    void InitOpenSSL(const Config& config);
    void DeinitOpenSSL();

    void InitHandshake();
    bool TryToHandshake();
    bool IsHandshakeTimeout();

    static openssl_bool CertificateCallback(int preverify_ok, X509_STORE_CTX* ctx);
    static void InfoCallback(const SSL* ssl, int where, int ret);

    static openssl_bool BioMethodNew(BIO* bio);
    static openssl_bool BioMethodFree(BIO* bio);
    static int BioMethodWrite(BIO* bio, const char* in, int in_size);
    static long BioMethodCtrl(BIO* bio, int cmd, long num, void* ptr);

    bool HandleVerify(const std::string& fingerprint);
    // void HandleInfo

    // 全局变量声明式
    static BIO_METHOD* bio_methods_;
    static int transport_ex_index_;
    static std::mutex global_mutex_;

    SSL_CTX* ctx_ = NULL;
    SSL* ssl_ = NULL;
    BIO* in_bio_ = NULL;
    BIO* out_bio_ = NULL;

protected:
    void Incoming(std::shared_ptr<Packet> in_packet) override;
    void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr) override;

private:
    Config config_;
    const bool is_client_;
    VerifyCallback verify_callback_;

    unsigned int curr_dscp_;
    static constexpr size_t DEFAULT_SSL_BUFFER_SIZE = 4096;

};

}

#endif