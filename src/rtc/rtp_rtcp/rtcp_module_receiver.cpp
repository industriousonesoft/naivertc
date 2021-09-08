#include "rtc/rtp_rtcp/rtcp_module.hpp"

namespace naivertc {

void RtcpModule::SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) {

}

void RtcpModule::OnRequestSendReport() {

}

void RtcpModule::OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) {

}

void RtcpModule::OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) {

}  
    
} // namespace naivertc
