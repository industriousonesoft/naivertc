#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"

namespace naivertc {
// RtcpContext
RtcpSender::RtcpContext::RtcpContext(const RtpSendFeedback& rtp_send_feedback,
                                     const RtcpReceiveFeedback& rtcp_receive_feedback,
                                     const uint16_t* nack_list,
                                     size_t nack_size,
                                     Timestamp now_time)
    : rtp_send_feedback(rtp_send_feedback),
      rtcp_receive_feedback(rtcp_receive_feedback),
      nack_list(nack_list),
      nack_size(nack_size),
      now_time(now_time) {}

// PacketSender
RtcpSender::PacketSender::PacketSender(MediaTransport* send_transport,
                                       bool is_audio,
                                       size_t max_packet_size)
    : send_transport_(send_transport), 
      is_audio_(is_audio),
      max_packet_size_(max_packet_size),
      index_(0) {
      assert(max_packet_size <= kIpPacketSize);
}

RtcpSender::PacketSender::~PacketSender() {}

size_t RtcpSender::PacketSender::max_packet_size() const {
    return max_packet_size_;
}
        
void RtcpSender::PacketSender::set_max_packet_size(size_t max_packet_size) {
    max_packet_size_ = max_packet_size;
}

void RtcpSender::PacketSender::AppendPacket(const RtcpPacket& packet) {
    packet.PackInto(buffer_, &index_, max_packet_size_, [this](CopyOnWriteBuffer packet){
        this->SendPacket(std::move(packet));
    });
}

void RtcpSender::PacketSender::Send() {
    if (index_ > 0) {
        SendPacket(CopyOnWriteBuffer(buffer_, &buffer_[index_]));
        index_ = 0;
    }
}

void RtcpSender::PacketSender::Reset() {
    index_ = 0;
}

void RtcpSender::PacketSender::SendPacket(CopyOnWriteBuffer packet) {
    PacketOptions options;
    options.kind = is_audio_ ? PacketKind::AUDIO : PacketKind::VIDEO;
    this->send_transport_->SendRtcpPacket(std::move(packet), std::move(options));
}
    
} // namespace naivertc
