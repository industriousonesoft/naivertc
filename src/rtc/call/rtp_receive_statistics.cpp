#include "rtc/call/rtp_receive_statistics.hpp"

namespace naivertc {
namespace {

constexpr int kDefaultMaxReorderingThreshold = 5;

} // namespace

RtpReceiveStatistics::RtpReceiveStatistics(Clock* clock) 
    : clock_(clock),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold) {}

RtpReceiveStatistics::~RtpReceiveStatistics() = default;

std::vector<rtcp::ReportBlock> RtpReceiveStatistics::GetRtcpReportBlocks(size_t max_blocks) {
    std::vector<rtcp::ReportBlock> report_blocks;
    report_blocks.reserve(std::min(max_blocks, ssrcs_.size()));

    for (size_t i = 0; i < ssrcs_.size() && report_blocks.size() < max_blocks; ++i) {
        auto statistician_it = statisticians_.find(ssrcs_[i]);
        if (statistician_it == statisticians_.end()) {
            continue;
        }
        auto report_block = statistician_it->second->GetReportBlock();
        if (report_block) {
            report_blocks.push_back(std::move(*report_block));
        }
    }
    return report_blocks;
}

void RtpReceiveStatistics::OnRtpPacket(RtpPacketReceived in_packet) {
    GetOrCreateStatistician(in_packet.ssrc())->OnRtpPacket(in_packet);
}

void RtpReceiveStatistics::SetMaxReorderingThreshold(int threshold) {
    max_reordering_threshold_ = threshold;
    for (auto& kv : statisticians_) {
        kv.second->set_max_reordering_threshold(threshold);
    }
}
    
void RtpReceiveStatistics::SetMaxReorderingThreshold(uint32_t ssrc,
                                                     int threshold) {
    GetOrCreateStatistician(ssrc)->set_max_reordering_threshold(threshold);
}
    
void RtpReceiveStatistics::EnableRetransmitDetection(uint32_t ssrc, bool enable) {
    GetOrCreateStatistician(ssrc)->set_enable_retransmit_detection(enable);
}

// Private methods
RtpStreamStatistician* RtpReceiveStatistics::GetOrCreateStatistician(uint32_t ssrc) {
    auto& statistician = statisticians_[ssrc];
    if (!statistician) {
        statistician = std::make_unique<RtpStreamStatistician>(ssrc, clock_, max_reordering_threshold_);
        ssrcs_.push_back(ssrc);
    }
    return statistician.get();
}
    
} // namespace naivertc
