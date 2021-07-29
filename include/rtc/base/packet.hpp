#ifndef _BASE_PACKET_H_
#define _BASE_PACKET_H_

#include "base/defines.hpp"

#include <memory>
#include <vector>

namespace naivertc {

using BinaryBuffer = std::vector<uint8_t>;

class RTC_CPP_EXPORT Packet : public BinaryBuffer, public std::enable_shared_from_this<Packet> {
public:
    static std::shared_ptr<Packet> Create() {
        return std::shared_ptr<Packet>(new Packet());
    }
    static std::shared_ptr<Packet> Create(const char* data, size_t size) {
        auto bytes = reinterpret_cast<const uint8_t*>(data);
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }
    static std::shared_ptr<Packet> Create(const uint8_t* bytes, size_t size) {
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }
    virtual ~Packet();

    size_t dscp() const;
    void set_dscp(size_t dscp);

protected:
    Packet();
    Packet(const uint8_t* bytes, size_t size);
    Packet(const Packet& other);
    Packet(const BinaryBuffer& buffer);
private:
    // Differentiated Services Code Point
    size_t dscp_;
};

}

#endif