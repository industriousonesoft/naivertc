#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"

#include <plog/Log.h>

namespace naivertc {

constexpr int kLocalMediaSsrcIndex = 1;
constexpr int kRtxSendSsrcIndex = 2;
constexpr int kFlexFecSsrcIndex = 3;

// RttStats
void RtcpReceiver::RttStats::AddRtt(TimeDelta rtt) {
  last_rtt_ = rtt;
  if (rtt < min_rtt_) {
    min_rtt_ = rtt;
  }
  if (rtt > max_rtt_) {
    max_rtt_ = rtt;
  }
  sum_rtt_ += rtt;
  ++num_rtts_;
}

// RTCPReceiver
RtcpReceiver::RtcpReceiver(const RtpRtcpInterface::Configuration& config, Observer* observer, std::shared_ptr<TaskQueue> task_queue) 
    : clock_(config.clock),
    observer_(observer),
    task_queue_(task_queue),
    receiver_only_(false) {
    if (!task_queue_) {
        task_queue_ = std::make_shared<TaskQueue>("RtcpReceiver.task.queue");
    }
    // Registered ssrcs
    registered_ssrcs_[kLocalMediaSsrcIndex] = config.local_media_ssrc;
    if (config.rtx_send_ssrc.has_value()) {
        registered_ssrcs_[kRtxSendSsrcIndex] = config.rtx_send_ssrc.value();
    }
    // TODO: insert flexfec ssrc if flexfec enabled
}

RtcpReceiver::~RtcpReceiver() {}

void RtcpReceiver::set_local_media_ssrc(uint32_t ssrc) {
    task_queue_->Async([this, ssrc](){
        registered_ssrcs_[kLocalMediaSsrcIndex] = ssrc;
    });
}

uint32_t RtcpReceiver::local_media_ssrc() const {
    return task_queue_->Sync<uint32_t>([this](){
        return registered_ssrcs_.at(kLocalMediaSsrcIndex);
    });
}

void RtcpReceiver::set_remote_ssrc(uint32_t ssrc) {
    task_queue_->Async([this, ssrc](){
        last_received_sr_ntp_.Reset();
        remote_ssrc_ = ssrc;
    });
}

uint32_t RtcpReceiver::remote_ssrc() const {
    return task_queue_->Sync<uint32_t>([this](){
        return remote_ssrc_;
    });
}

void RtcpReceiver::IncomingPacket(BinaryBuffer packet) {
    task_queue_->Async([this, packet = std::move(packet)](){
        if (ParseCompoundPacket(packet)) {
            PLOG_WARNING << "Failed to parse incoming packet.";
            return;
        }
    });
    
}

// Private methods
bool RtcpReceiver::IsRegisteredSsrc(uint32_t ssrc) const {
    for (const auto& kv : registered_ssrcs_) {
        if (kv.second == ssrc) {
            return true;
        }
    }
    return false;
}

} // namespace naivertc
