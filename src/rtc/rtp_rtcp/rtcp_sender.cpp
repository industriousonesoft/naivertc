#include "rtc/rtp_rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp_packet.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
const uint32_t kRtcpAnyExtendedReports = kRtcpXrReceiverReferenceTime |
                                         kRtcpXrDlrrReportBlock |
                                         kRtcpXrTargetBitrate;
constexpr int32_t kDefaultVideoReportInterval = 1000;
constexpr int32_t kDefaultAudioReportInterval = 5000;
}  // namespace

// Helper to put several RTCP packets into lower layer datagram RTCP packet.
class RtcpSender::PacketSender {
public:
    PacketSender(RtcpPacket::PacketReadyCallback callback,
                size_t max_packet_size)
        : callback_(callback), max_packet_size_(max_packet_size) {
            assert(max_packet_size <= kIpPacketSize);
    }
    ~PacketSender() {}

    // Appends a packet to pending compound packet.
    // Sends rtcp packet if buffer is full and resets the buffer.
    void AppendPacket(const RtcpPacket& packet) {
        packet.PackInto(buffer_, &index_, max_packet_size_, callback_);
    }

    // Sends pending rtcp packet.
    void Send() {
        if (index_ > 0) {
            callback_(BinaryBuffer(buffer_, &buffer_[index_]));
            index_ = 0;
        }
    }

private:
    const RtcpPacket::PacketReadyCallback callback_;
    const size_t max_packet_size_;
    size_t index_ = 0;
    uint8_t buffer_[kIpPacketSize];
};

// RtcpSender implements
RtcpSender::RtcpSender(Configuration config, std::shared_ptr<TaskQueue> task_queue) 
    : audio_(config.audio),
    ssrc_(config.local_media_ssrc),
    clock_(config.clock),
    task_queue_(task_queue) {
    
    if (!task_queue_) {
        task_queue_ = std::make_shared<TaskQueue>("RtcpSender.task.queue");
    }
}

RtcpSender::~RtcpSender() {}

// Private methods
void RtcpSender::SetFlag(uint32_t type, bool is_volatile) {
    if (type & kRtcpAnyExtendedReports) {
        report_flags_.insert(ReportFlag(kRtcpAnyExtendedReports, is_volatile));
    } else {
        report_flags_.insert(ReportFlag(type, is_volatile));
    }
}

bool RtcpSender::IsFlagPresent(uint32_t type) const {
    return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
}

bool RtcpSender::ConsumeFlag(uint32_t type, bool forced) {
    auto it = report_flags_.find(ReportFlag(type, false));
    if (it == report_flags_.end())
        return false;
    if (it->is_volatile || forced)
        report_flags_.erase((it));
    return true;
}

bool RtcpSender::AllVolatileFlagsConsumed() const {
    for (const ReportFlag& flag : report_flags_) {
        if (flag.is_volatile)
        return false;
    }
    return true;
}
    
} // namespace naivertc
