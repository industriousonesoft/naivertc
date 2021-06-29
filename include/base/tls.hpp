#ifndef _BASE_TLS_H_
#define _BASE_TLS_H_

#ifdef _WIN32
// Include winsock2.h header first since OpenSSL may include winsock.h
#include <winsock2.h>
#endif

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <string>

#ifndef BIO_EOF
#define BIO_EOF -1
#endif

namespace naivertc::openssl {

void init();
std::string error_string(unsigned long err);

bool check(int success, const std::string &message = "OpenSSL error");
bool check(SSL *ssl, int ret, const std::string &message = "OpenSSL error");

}

#endif