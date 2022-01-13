#ifndef _RTC_RTP_RTCP_COMPONENTS_RECEIVE_STATISTICS_H_
#define _RTC_RTP_RTCP_COMPONENTS_RECEIVE_STATISTICS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/call/rtp_stream_statistician.hpp"

#include <vector>
#include <unordered_map>

namespace naivertc {

// RtpReceiveStatistics
class RTC_CPP_EXPORT RtpReceiveStatistics : public RtcpReportBlockProvider {
public:
    RtpReceiveStatistics(Clock* clock);
    ~RtpReceiveStatistics() override;

    void SetMaxReorderingThreshold(int threshold);
    void SetMaxReorderingThreshold(uint32_t ssrc,
                                   int threshold);
    void EnableRetransmitDetection(uint32_t ssrc, 
                                   bool enable);

    RtpStreamStatistician* GetStatistician(uint32_t ssrc) const;

    void OnRtpPacket(const RtpPacketReceived& in_packet);

    // Implements RtcpReportBlockProvider
    std::vector<rtcp::ReportBlock> GetRtcpReportBlocks(size_t max_blocks) override;

private:
    RtpStreamStatistician* GetOrCreateStatistician(uint32_t ssrc);

private:
    Clock* const clock_;
    size_t last_returned_ssrc_idx_;
    int max_reordering_threshold_;
    std::vector<uint32_t> ssrcs_;
    std::unordered_map<uint32_t, std::unique_ptr<RtpStreamStatistician>> statisticians_;
};
    
} // namespace naivertc


#endif