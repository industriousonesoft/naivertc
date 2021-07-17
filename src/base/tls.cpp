#include "base/tls.hpp"

#include <plog/Log.h>

namespace naivertc::openssl {

void init() {
	static std::mutex mutex;
	static bool done = false;

	std::lock_guard lock(mutex);
	if (!std::exchange(done, true)) {
		OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
		OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
	}
}

std::string error_string(unsigned long err) {
	const size_t bufferSize = 256;
	char buffer[bufferSize];
	ERR_error_string_n(err, buffer, bufferSize);
	return std::string(buffer);
}

bool check(int success, const std::string& message) {
	if (success)
		return true;

	std::string str = error_string(ERR_get_error());
	PLOG_ERROR << message << ": " << str;
	throw std::runtime_error(message + ": " + str);
}

bool check(SSL *ssl, int ret, const std::string& message) {
	if (ret == BIO_EOF)
		return true;

	unsigned long err = SSL_get_error(ssl, ret);
	if (err == SSL_ERROR_NONE || err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
		return true;
	}
	if (err == SSL_ERROR_ZERO_RETURN) {
		PLOG_DEBUG << "DTLS connection cleanly closed";
		return false;
	}
	std::string str = error_string(err);
	PLOG_ERROR << str;
	throw std::runtime_error(message + ": " + str);
}

} // namespace naivertc::openssl