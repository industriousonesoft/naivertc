#ifndef _PC_RTP_PACKET_H_
#define _PC_RTP_PACKET_H_

#include "base/defines.hpp"
#include "base/packet.hpp"

#include <memory>

namespace {
using SSRCId = unsigned int;
}

namespace naivertc {
    
class RTC_CPP_EXPORT RtpPacket : public std::enable_shared_from_this<RtpPacket> {
public:
    enum class Type {
        RTP,
        RTCP
    };
public:
    static std::shared_ptr<RtpPacket> Create(std::shared_ptr<Packet> raw_packet, Type type, SSRCId ssrc_id) {
        return std::shared_ptr<RtpPacket>(new RtpPacket(std::move(raw_packet), type, ssrc_id));
    }

    ~RtpPacket();

    Type type() const { return type_; }
    SSRCId ssrc_id() const { return ssrc_id_; }

    const char* data() const;
    char* data();
    size_t size() const;
    const std::vector<std::byte> bytes() const;

    bool is_empty() const;

protected:
    RtpPacket(std::shared_ptr<Packet> raw_packet, Type type, SSRCId ssrc_id);
    
private:
    std::shared_ptr<Packet> raw_packet_{nullptr};
    Type type_;
    SSRCId ssrc_id_;
};

} // namespace naivertc


#endif