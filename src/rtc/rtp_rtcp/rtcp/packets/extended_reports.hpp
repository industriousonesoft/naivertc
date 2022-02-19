#ifndef _RTC_RTP_RTCP_RTCP_PACKTES_EXTENDED_REPORTS_H_
#define _RTC_RTP_RTCP_RTCP_PACKTES_EXTENDED_REPORTS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/rrtr.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/dlrr.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/target_bitrate.hpp"

#include <optional>

namespace naivertc {
namespace rtcp {

class CommonHeader;

// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
class ExtendedReports : public RtcpPacket {
public:
    static const uint8_t kPacketType = 207;
    // FIXME: Why the max number of sub blocks is 50? How to calculate it?
    static const size_t kMaxNumberOfDlrrTimeInfos = 50;
public:
    ExtendedReports();
    ~ExtendedReports() override;

    const std::optional<Rrtr>& rrtr() const;
    const Dlrr& dlrr() const;
    const std::optional<TargetBitrate>& target_bitrate() const;

    void set_rrtr(const Rrtr& rrtr);
    bool AddDlrrTimeInfo(const Dlrr::TimeInfo& block);
    void set_target_bitrate(const TargetBitrate& bitrate);

    bool Parse(const CommonHeader& packet);

    size_t PacketSize() const override;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    size_t RrtrBlockSize() const;
    size_t DlrrBlockSize() const;
    size_t TargetBitrateBlockSize() const;

    void ParseRrtrBlock(const uint8_t* buffer, size_t size);
    void ParseDlrrBlock(const uint8_t* buffer, size_t size);
    void ParseTaragetBitrateBlock(const uint8_t* buffer, size_t size);

private:
    static const size_t kXrBaseSize = 4;

    std::optional<Rrtr> rrtr_block_;
    std::optional<TargetBitrate> target_bitrate_;
    Dlrr dlrr_block_;
};


} // namespace rtcp
} // namespace naivertc


#endif