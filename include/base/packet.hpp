#ifndef _BASE_PACKET_H_
#define _BASE_PACKET_H_

#include "base/defines.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT Packet : public std::enable_shared_from_this<Packet> {
public:
    ~Packet();
};

}

#endif