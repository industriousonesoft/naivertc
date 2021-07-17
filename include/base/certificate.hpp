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

class RTC_CPP_EXPORT Certificate {
public:
    static std::future<std::shared_ptr<Certificate>> MakeCertificate(CertificateType type = CertificateType::DEFAULT);
public:
    Certificate(std::string_view crt_pem, std::string_view key_pem);
    Certificate(std::shared_ptr<X509> x509, std::shared_ptr<EVP_PKEY> pkey);
    std::tuple<X509 *, EVP_PKEY *> credentials() const;

    ~Certificate();

    const std::string fingerprint() const;
    std::string fingerprint();

    static std::string MakeFingerprint(X509* x509);

private:
    static std::shared_ptr<Certificate> Generate(CertificateType type, std::string_view common_name);
private:
    std::shared_ptr<X509> x509_;
    std::shared_ptr<EVP_PKEY> pkey_;

    std::string fingerprint_;

};

}

#endif