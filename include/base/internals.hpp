#ifndef _BASE_INTERNALS_H_
#define _BASE_INTERNALS_H_

#include <stddef.h>

namespace naivertc {

const size_t DEFAULT_SCTP_PORT = 5000;

const size_t DEFAULT_LOCAL_MAX_MESSAGE_SIZE = 256 * 1024;

// IPv6 minimum guaranteed MTU
const size_t DEFAULT_MTU_SIZE = 1280;

}

#endif