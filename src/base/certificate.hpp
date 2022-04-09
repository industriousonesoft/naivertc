#ifndef _BASE_CERTIFICATE_H_
#define _BASE_CERTIFICATE_H_

#include "base/defines.hpp"
#include "base/tls.hpp"
#include "rtc/pc/peer_connection_configuration.hpp"

#include <string>
#include <memory>
#include <tuple>
#include <future>

namespace naivertc {

class Certificate {
public:
    static std::shared_future<std::shared_ptr<Certificate>> MakeCertificate(CertificateType type = CertificateType::DEFAULT);
public:
    Certificate(std::string_view crt_pem, std::string_view key_pem);
#if !defined(USE_MBEDTLS)
    Certificate(std::shared_ptr<X509> x509, std::shared_ptr<EVP_PKEY> pkey);
    std::tuple<X509 *, EVP_PKEY *> credentials() const;
#endif

    ~Certificate();

    const std::string fingerprint() const;
    std::string fingerprint();
#if !defined(USE_MBEDTLS)
    static std::string MakeFingerprint(X509* x509);
#endif

private:
    static std::shared_ptr<Certificate> Generate(CertificateType type, std::string_view common_name);
private:
#if !defined(USE_MBEDTLS)
    std::shared_ptr<X509> x509_;
    std::shared_ptr<EVP_PKEY> pkey_;
#endif

    std::string fingerprint_;

};

}

#endif