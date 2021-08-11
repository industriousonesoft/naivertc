#ifndef _RTC_BASE_PACKET_H_
#define _RTC_BASE_PACKET_H_

#include "base/defines.hpp"

#include <memory>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT Packet : public BinaryBuffer, public std::enable_shared_from_this<Packet> {
public:
    static std::shared_ptr<Packet> Create(size_t capacity) {
        return std::shared_ptr<Packet>(new Packet(capacity));
    }
    static std::shared_ptr<Packet> Create(const char* data, size_t size) {
        auto bytes = reinterpret_cast<const uint8_t*>(data);
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }
    static std::shared_ptr<Packet> Create(const uint8_t* bytes, size_t size) {
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }
public:
    Packet(size_t capacity);
    Packet(const uint8_t* bytes, size_t size);
    Packet(const BinaryBuffer& raw_packet);
    Packet(BinaryBuffer&& raw_packet);

    virtual ~Packet();

    size_t dscp() const;
    void set_dscp(size_t dscp);
 
private:
    // Differentiated Services Code Point
    size_t dscp_;
};

}

#endif