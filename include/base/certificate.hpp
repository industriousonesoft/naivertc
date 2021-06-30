#ifndef _BASE_CERTIFICATE_H_
#define _BASE_CERTIFICATE_H_

#include "base/defines.hpp"
#include "base/tls.hpp"
#include "pc/peer_connection_configuration.hpp"

#include <string>
#include <memory>
#include <tuple>
#include <future>

namespace naivertc {

class RTC_CPP_EXPORT Certificate {
// public:
    // static std::shared_future<std::shared_ptr<Certificate>> MakeCertificate(CertificateType type = CertificateType::DEFAULT);
public:
    Certificate(std::string crt_pem, std::string key_pem);
    Certificate(std::shared_ptr<X509> x509, std::shared_ptr<EVP_PKEY> pkey);
    std::tuple<X509 *, EVP_PKEY *> credentials() const;

    ~Certificate();

    std::string fingerprint() const;

private:
    std::string MakeFingerprint(X509* x509);
    static std::shared_ptr<Certificate> Generate(CertificateType type, const std::string common_name);
private:
    std::shared_ptr<X509> x509_;
    std::shared_ptr<EVP_PKEY> pkey_;

    std::string fingerprint_;

};

}

#endif