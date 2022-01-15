#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_REMB_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_REMB_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/psfb.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {

class CommonHeader;

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
// See https://datatracker.ietf.org/doc/html/draft-alvestrand-rmcat-remb-03
class RTC_CPP_EXPORT Remb : public Psfb {
public:
    static constexpr size_t kMaxNumberOfSsrcs = 0xffu;
public:
    Remb();
    Remb(const Remb&);
    ~Remb();

    int64_t bitrate_bps() const { return bitrate_bps_; }
    void set_bitrate_bps(int64_t bitrate_bps) { bitrate_bps_ = bitrate_bps; }

    const std::vector<uint32_t>& ssrcs() const { return ssrcs_; }
    bool set_ssrcs(std::vector<uint32_t> ssrcs);

    size_t PacketSize() const override;
    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

    bool Parse(const CommonHeader& packet);

private:
    // Media ssrc is unsed, shadow base class setter and getter.
    void set_media_ssrc(uint32_t);
    uint32_t media_ssrc() const;

private:
    static constexpr uint32_t kUniqueIdentifier = 0x52454D42; // 'R' 'E' 'M' 'B'
    static constexpr size_t kRembBaseSize = 16;

    int64_t bitrate_bps_;
    std::vector<uint32_t> ssrcs_;
};
    
} // namespace rtcp
} // namespace naivertc

#endif