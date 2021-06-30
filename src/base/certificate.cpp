#include "base/certificate.hpp"

#include <plog/Log.h>

#include <sstream>
#include <iomanip>

namespace naivertc {

Certificate::Certificate(std::string crt_pem, std::string key_pem) {
    BIO *bio = BIO_new(BIO_s_mem());
	BIO_write(bio, crt_pem.data(), int(crt_pem.size()));
	x509_ = std::shared_ptr<X509>(PEM_read_bio_X509(bio, nullptr, 0, 0), X509_free);
	BIO_free(bio);
	if (!x509_)
		throw std::invalid_argument("Unable to import certificate PEM");

	bio = BIO_new(BIO_s_mem());
	BIO_write(bio, key_pem.data(), int(key_pem.size()));
	pkey_ = std::shared_ptr<EVP_PKEY>(PEM_read_bio_PrivateKey(bio, nullptr, 0, 0), EVP_PKEY_free);
	BIO_free(bio);
	if (!x509_)
		throw std::invalid_argument("Unable to import PEM key PEM");

	fingerprint_ = MakeFingerprint(x509_.get());
}

Certificate::Certificate(std::shared_ptr<X509> x509, std::shared_ptr<EVP_PKEY> pkey) 
    : x509_(x509),
    pkey_(pkey) {
    fingerprint_ = MakeFingerprint(x509.get());
}

std::tuple<X509 *, EVP_PKEY *> Certificate::credentials() const {
    return {x509_.get(), pkey_.get()};
}

Certificate::~Certificate() {
    x509_.reset();
    pkey_.reset();
}

std::string Certificate::fingerprint() const {
    return fingerprint_;
}

std::string Certificate::MakeFingerprint(X509* x509) {
    // SHA_265的长度为32个字节
    const size_t size = 32;
    unsigned char buffer[size];
    unsigned int len = size;
    if (!X509_digest(x509, EVP_sha256(), buffer, &len)) {
        throw std::runtime_error("Failed to create SHA-256 fingerprint of X509 certificate.");
    }

    std::ostringstream oss;
    // hex + uppercast + filled with '0
    oss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            oss << std::setw(1) << ':';
        } 
        oss << std::setw(2) << unsigned(buffer[i]);
    }
    return oss.str();
}

const std::string COMMON_NAME = "libnaivertc";
std::shared_ptr<Certificate> Certificate::Generate(CertificateType type, const std::string common_name) {

    PLOG_DEBUG << "Generating certificate with OpenSSL.";

    std::shared_ptr<X509> x509(X509_new(), X509_free);
    std::shared_ptr<EVP_PKEY> pkey(EVP_PKEY_new(), EVP_PKEY_free);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> serial_number(BN_new(), BN_free);
    std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> name(X509_NAME_new(), X509_NAME_free);

    if (!x509 || !pkey || !serial_number || !name) {
        throw std::runtime_error("Unable to allocate structures for certificate generation.");
    }

    switch (type) {
    // RFC 8827 WebRTC Security Architecture 6.5. Communications Security
	// All implementations MUST support DTLS 1.2 with the TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
	// cipher suite and the P-256 curve
	// See https://tools.ietf.org/html/rfc8827#section-6.5
    case CertificateType::DEFAULT:
    case CertificateType::ECDSA: {
        PLOG_VERBOSE << "Generating ECDSA P-256 key pair";

        std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> ecc(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), EC_KEY_free);

        if (!ecc) {
            throw std::runtime_error("Unabale to allocate structure for ECDSA P-256 key pair.");
        }

        // Set ASN1 OID
        EC_KEY_set_asn1_flag(ecc.get(), OPENSSL_EC_NAMED_CURVE);

        if (!EC_KEY_generate_key(ecc.get()) || 
            !EVP_PKEY_assign_EC_KEY(pkey.get(), ecc.release())) {
            throw std::runtime_error("Unable to generate ECDSA P-256 key pair.");
        }

        break;
    }
    case CertificateType::RSA: {
        PLOG_VERBOSE << "Generateing RAS key pair.";

        const int bits = 2048;
        const unsigned int e = 65537; // 2^16 + 1

        std::unique_ptr<RSA, decltype(&RSA_free)> rsa(RSA_new(), RSA_free);
		std::unique_ptr<BIGNUM, decltype(&BN_free)> exponent(BN_new(), BN_free);
		if (!rsa || !exponent) {
			throw std::runtime_error("Unable to allocate structures for RSA key pair.");
        }

		if (!BN_set_word(exponent.get(), e) ||
		    !RSA_generate_key_ex(rsa.get(), bits, exponent.get(), NULL) ||
		    !EVP_PKEY_assign_RSA(pkey.get(), rsa.release())) {
			throw std::runtime_error("Unable to generate RSA key pair.");
        }

        break;
    }
    default:
        throw std::invalid_argument("Unknown certificate type");
        break;
    }

    const size_t serialSize = 16;
	auto *commonNameBytes = reinterpret_cast<unsigned char *>(const_cast<char *>(common_name.c_str()));

	if (!X509_set_pubkey(x509.get(), pkey.get()))
		throw std::runtime_error("Unable to set certificate public key");

	if (!X509_gmtime_adj(X509_getm_notBefore(x509.get()), 3600 * -1) ||
	    !X509_gmtime_adj(X509_getm_notAfter(x509.get()), 3600 * 24 * 365) ||
	    !X509_set_version(x509.get(), 1) ||
	    !BN_pseudo_rand(serial_number.get(), serialSize, 0, 0) ||
	    !BN_to_ASN1_INTEGER(serial_number.get(), X509_get_serialNumber(x509.get())) ||
	    !X509_NAME_add_entry_by_NID(name.get(), NID_commonName, MBSTRING_UTF8, commonNameBytes, -1,
	                                -1, 0) ||
	    !X509_set_subject_name(x509.get(), name.get()) ||
	    !X509_set_issuer_name(x509.get(), name.get()))
		throw std::runtime_error("Unable to set certificate properties");

	if (!X509_sign(x509.get(), pkey.get(), EVP_sha256()))
		throw std::runtime_error("Unable to auto-sign certificate");

	return std::make_shared<Certificate>(x509, pkey);
}

// std::shared_future<std::shared_ptr<Certificate>> Certificate::MakeCertificate(CertificateType type) {

// }

}