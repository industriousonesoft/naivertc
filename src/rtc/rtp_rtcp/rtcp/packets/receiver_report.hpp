#ifndef _RTC_RTP_RTCP_RTCP_PACKTES_RECEIVER_REPORT_H_
#define _RTC_RTP_RTCP_RTCP_PACKTES_RECEIVER_REPORT_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {

class CommonHeader;

class RTC_CPP_EXPORT ReceiverReport : public RtcpPacket {
public:
    static constexpr uint8_t kPacketType = 201;
    static constexpr size_t kMaxNumberOfReportBlocks = 0x1F;

public:
    ReceiverReport();
    ReceiverReport(const ReceiverReport&);
    ~ReceiverReport();

    const std::vector<ReportBlock>& report_blocks() const {
        return report_blocks_;
    }

    bool Parse(const CommonHeader& packet);

    bool AddReportBlock(const ReportBlock& block);
    bool SetReportBlocks(std::vector<ReportBlock> blocks);

    size_t PacketSize() const override;
    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    static constexpr size_t kReceiverReportBaseSize = 4;

    std::vector<ReportBlock> report_blocks_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif