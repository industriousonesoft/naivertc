#ifndef _RTC_RTP_RTCP_RTCP_PACKERTS_TMMB_ITEM_H_
#define _RTC_RTP_RTCP_RTCP_PACKERTS_TMMB_ITEM_H_

#include "base/defines.hpp"

namespace naivertc {
namespace rtcp {

// RFC5104, Section 3.5.4
// Temporary Maximum Media Stream Bitrate Request/Notification.
// Used both by TMMBR and TMMBN rtcp packets.
class RTC_CPP_EXPORT TmmbItem {
public:
    static const size_t kFixedTmmbItemSize = 8;

    TmmbItem() : ssrc_(0), bitrate_bps_(0), packet_overhead_(0) {}
    TmmbItem(uint32_t ssrc, uint64_t bitrate_bps, uint16_t overhead);

    bool Parse(const uint8_t* buffer);
    bool PackInto(uint8_t* buffer, size_t size) const;

    void set_ssrc(uint32_t ssrc) { ssrc_ = ssrc; }
    void set_bitrate_bps(uint64_t bitrate_bps) { bitrate_bps_ = bitrate_bps; }
    void set_packet_overhead(uint16_t overhead);

    uint32_t ssrc() const { return ssrc_; }
    uint64_t bitrate_bps() const { return bitrate_bps_; }
    uint16_t packet_overhead() const { return packet_overhead_; }

private:
    // Media stream id.
    uint32_t ssrc_;
    // Maximum total media bit rate that the media receiver is
    // currently prepared to accept for this media stream.
    uint64_t bitrate_bps_;
    // Per-packet overhead that the media receiver has observed
    // for this media stream at its chosen reference protocol layer.
    uint16_t packet_overhead_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif