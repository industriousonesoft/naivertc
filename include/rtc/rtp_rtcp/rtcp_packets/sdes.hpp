#ifndef _RTC_RTCP_SOURCE_DESCRIPTION_H_
#define _RTC_RTCP_SOURCE_DESCRIPTION_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp_packet.hpp"

#include <string>
#include <vector>

namespace naivertc {
namespace rtcp {

class CommonHeader;

class RTC_CPP_EXPORT Sdes : public RtcpPacket {
public:
    struct Chunk {
       uint32_t ssrc;
       std::string cname;
    };
    static constexpr uint8_t kPacketType = 202;
    static constexpr size_t kMaxNumberOfChunks = 0x1F;
public:
    Sdes();
    ~Sdes() override;

    const std::vector<Chunk>& chunks() const { return chunks_; }

    bool AddCName(uint32_t ssrc, std::string cname);

    bool Parse(const CommonHeader& packet);

    size_t PacketSize() const override;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    std::vector<Chunk> chunks_;
    size_t packet_size_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif