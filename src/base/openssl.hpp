#ifndef _BASE_OPENSSL_H_
#define _BASE_OPENSSL_H_

#ifdef _WIN32
// Include winsock2.h header first since OpenSSL may include winsock.h
#include <winsock2.h>
#endif // _WIN32

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#endif