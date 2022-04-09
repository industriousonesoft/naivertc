/* mbed TLS feature support */
#define MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED 
#define MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED 
#define MBEDTLS_SSL_PROTO_TLS1_2

/* mbed TLS modules */
#define MBEDTLS_AES_C                   // 启用AES加密
#define MBEDTLS_CCM_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CAMELLIA_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_MD_C
#define MBEDTLS_CTR_DRBG_C 
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_BASE64_C

/* Save RAM at the expense of ROM */
#define MBEDTLS_AES_ROM_TABLES

/*
 * You should adjust this to the exact number of sources you're using: default
 * is the "platform_entropy_poll" source, but you may want to add other ones
 * Minimum is 2 for the entropy test suite.
 */
#define MBEDTLS_ENTROPY_MAX_SOURCES 2

/*
 * Save RAM at the expense of interoperability: do this only if you control
 * both ends of the connection!  (See comments in "mbedtls/ssl.h".)
 * The optimal size here depends on the typical size of records.
 */
#define MBEDTLS_SSL_IN_CONTENT_LEN             1024
#define MBEDTLS_SSL_OUT_CONTENT_LEN             1024

/* Verify certificates throuth a callback instead of a linked list. */
#define MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK