#ifndef _RTC_RTP_RTCP_COMPONENTS_RECEIVE_STATISTICS_H_
#define _RTC_RTP_RTCP_COMPONENTS_RECEIVE_STATISTICS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"
#include "rtc/api/rtp_packet_sink.hpp"

#include <vector>

namespace naivertc {

// RtcpReportBlockProvider
class RtcpReportBlockProvider {
public:
    virtual ~RtcpReportBlockProvider() = default;
    virtual std::vector<rtcp::ReportBlock> GetRtcpReportBlocks(size_t max_blocks) = 0;
};

// RtpReceiveStatistics
class RTC_CPP_EXPORT RtpReceiveStatistics : public RtcpReportBlockProvider,
                                            public RtpPacketSink {
public:
    RtpReceiveStatistics();
    ~RtpReceiveStatistics() override;

    // Implements RtcpReportBlockProvider
    std::vector<rtcp::ReportBlock> GetRtcpReportBlocks(size_t max_blocks) override;

    // Implements RtpPacketSink
    void OnRtpPacket(RtpPacketReceived in_packet) override;
};
    
} // namespace naivertc


#endif