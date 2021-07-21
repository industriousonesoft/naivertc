#ifndef _BASE_PACKET_H_
#define _BASE_PACKET_H_

#include "base/defines.hpp"

#include <memory>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT Packet : public std::enable_shared_from_this<Packet> {
public:
    static std::shared_ptr<Packet> Create(const char* data, size_t size) {
        auto bytes = reinterpret_cast<const uint8_t*>(data);
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }
    static std::shared_ptr<Packet> Create(const uint8_t* bytes, size_t size) {
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }
    virtual ~Packet();

    size_t size() const;
    const std::vector<uint8_t> bytes() const;
    const uint8_t* data() const;
    uint8_t* data();

    unsigned int dscp() const { return dscp_; };
    void set_dscp(unsigned int dscp) { dscp_ = dscp; }

    bool is_empty() const { return bytes_.empty(); }

    void Resize(size_t new_size);

protected:
    Packet(const uint8_t* bytes, size_t size);
    Packet(std::vector<uint8_t>&& bytes);
private:
    std::vector<uint8_t> bytes_;

    // Differentiated Services Code Point
    unsigned int dscp_;
};

}

#endif