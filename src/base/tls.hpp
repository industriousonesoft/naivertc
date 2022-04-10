#ifndef _BASE_TLS_H_
#define _BASE_TLS_H_

#include <string>

#if defined(USE_MBEDTLS)
// Use mbedtls
#include "base/mbedtls.hpp"

namespace naivertc::mbedtls {

std::string error_string(int err_code);

bool check(int ret, const std::string &message = "MbedTLS error");

}

#else
// Use openssl
#include "base/openssl.hpp"

#ifndef BIO_EOF
#define BIO_EOF -1
#endif // BIO_EOF

// openssl
namespace naivertc::openssl {

void init();
std::string error_string(unsigned long err);

bool check(int success, const std::string &message = "OpenSSL error");
bool check(SSL *ssl, int ret, const std::string &message = "OpenSSL error");

}

#endif // defined(USE_MBEDTLS)

#endif // _BASE_TLS_H_