#ifndef _RTC_RTCP_PACKET_H_
#define _RTC_RTCP_PACKET_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT RtcpPacket {
public:
    // Callback used to signal that an RTCP packet is ready. Note that this 
    // may not contain all data in the RtcpPacket; if a packet can not fit in 
    // max_length bytes, it will be fragmented and multiple calls to this 
    // callback will be made.
    using PacketReadyCallback = std::function<void(BinaryBuffer)>;

    virtual ~RtcpPacket() = default;

    uint32_t sender_ssrc() const { return sender_ssrc_; }
    void set_sender_ssrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }

    // Size of this packet in bytes including headers
    virtual size_t PacketSize() const = 0;

    // Pack data into the given buffer at the given position.
    virtual bool PackInto(uint8_t* buffer,
                          size_t* index,
                          size_t max_length,
                          PacketReadyCallback callback) const  = 0;

    bool Build(size_t max_length, PacketReadyCallback callback) const;

    // Convenience method mostly used for test. Creates packet without
    // fragmentation using BlockSize() to allocate big enough buffer.
    BinaryBuffer Build() const;

protected:
    // Size of the RTCP common header
    static constexpr size_t kFixedRtcpCommonHeaderSize = 4;
    RtcpPacket() {}

    static void CreateCommonHeader(size_t count_or_format, // Depends on packet type
                                    uint8_t packet_type,
                                    size_t payload_size,
                                    uint8_t* buffer,
                                    size_t* pos);

    static void CreateCommonHeader(size_t count_or_format,
                                    uint8_t packet_type,
                                    size_t payload_size, 
                                    bool padding,
                                    uint8_t* buffer,
                                    size_t* pos);

    bool OnBufferFull(uint8_t* buffer, size_t* index, PacketReadyCallback callback) const;

    size_t PayloadSize() const;
    
private:
    uint32_t sender_ssrc_ = 0;

};
    
} // namespace naivertc


#endif