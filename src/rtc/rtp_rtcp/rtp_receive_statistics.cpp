#include "rtc/rtp_rtcp/rtp_receive_statistics.hpp"

namespace naivertc {

RtpReceiveStatistics::RtpReceiveStatistics() {}

RtpReceiveStatistics::~RtpReceiveStatistics() = default;

std::vector<rtcp::ReportBlock> RtpReceiveStatistics::GetRtcpReportBlocks(size_t max_blocks) {}

void RtpReceiveStatistics::OnRtpPacket(RtpPacketReceived in_packet) {}
    
} // namespace naivertc
