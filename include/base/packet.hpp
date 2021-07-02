#ifndef _BASE_PACKET_H_
#define _BASE_PACKET_H_

#include "base/defines.hpp"

#include <memory>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT Packet : public std::enable_shared_from_this<Packet> {
    Packet(const char* data, size_t size);
public:
    static std::shared_ptr<Packet> Create(const char* data, size_t size) {
        return std::shared_ptr<Packet>(new Packet(data, size));
    }
    ~Packet();

    const char* data() const;
    size_t size() const;

    unsigned int dscp() const { return dscp_; };
    void set_dscp(unsigned int dscp) { dscp_ = dscp; }

private:
    std::vector<std::byte> bytes_;

    // Differentiated Services Code Point
    unsigned int dscp_;
};

}

#endif