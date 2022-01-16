#include "rtc/rtp_rtcp/components/rtp_receive_statistics.hpp"

namespace naivertc {
namespace {

constexpr int kDefaultMaxReorderingThreshold = 5;

} // namespace

RtpReceiveStatistics::RtpReceiveStatistics(Clock* clock) 
    : clock_(clock),
      last_returned_ssrc_idx_(0),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold) {}

RtpReceiveStatistics::~RtpReceiveStatistics() = default;

std::vector<rtcp::ReportBlock> RtpReceiveStatistics::GetRtcpReportBlocks(size_t max_blocks) {
    std::vector<rtcp::ReportBlock> report_blocks;
    report_blocks.reserve(std::min(max_blocks, ssrcs_.size()));

    size_t ssrc_idx = 0;
    for (size_t i = 0; i < ssrcs_.size() && report_blocks.size() < max_blocks; ++i) {
        ssrc_idx = (last_returned_ssrc_idx_ + i + 1) % ssrcs_.size();
        auto statistician_it = statisticians_.find(ssrcs_[ssrc_idx]);
        if (statistician_it == statisticians_.end()) {
            continue;
        }
        auto report_block = statistician_it->second->GetReportBlock();
        if (report_block) {
            report_blocks.push_back(std::move(*report_block));
        }
    }
    last_returned_ssrc_idx_ = ssrc_idx;
    return report_blocks;
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

RtpStreamStatistician* RtpReceiveStatistics::GetStatistician(uint32_t ssrc) const {
    const auto& it = statisticians_.find(ssrc);
    if (it == statisticians_.end()) {
        return nullptr;
    }
    return it->second.get();
}

void RtpReceiveStatistics::OnRtpPacket(const RtpPacketReceived& in_packet) {
    GetOrCreateStatistician(in_packet.ssrc())->OnRtpPacket(in_packet);
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
