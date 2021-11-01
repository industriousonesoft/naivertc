#ifndef _RTC_BASE_PACKET_H_
#define _RTC_BASE_PACKET_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

#include <memory>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT Packet : public CopyOnWriteBuffer {
public:
    Packet(size_t capacity);
    Packet(const uint8_t* bytes, size_t size);
    Packet(const CopyOnWriteBuffer& raw_packet);
    Packet(CopyOnWriteBuffer&& raw_packet);
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