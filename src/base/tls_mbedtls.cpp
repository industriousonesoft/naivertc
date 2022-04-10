#if defined(USE_MBEDTLS)

#include "base/tls.hpp"

#include <plog/Log.h>

namespace naivertc::mbedtls {

std::string error_string(int err_code) {
	const size_t bufferSize = 256;
	char buffer[bufferSize];
    mbedtls_strerror(err_code, buffer, bufferSize);
	return std::string(buffer);
}

bool check(int ret, const std::string& message) {
    if (ret == 0) {
        return true;
    }
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		return false;
	}
    // if(ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
    //     PLOG_WARNING << "Hello verification requested.";
    //     return false;
    // }
	if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
		PLOG_DEBUG << "DTLS connection timeouted.";
		return false;
	}
    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		PLOG_DEBUG << "DTLS connection cleanly closed.";
		return false;
	}
	std::string str = error_string(ret);
	throw std::runtime_error(message + ": " + str);
}

} // namespace naivertc::mbedtls

#endif // !defined(USE_MBEDTLS)