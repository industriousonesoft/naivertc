#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"

namespace naivertc {
// RtcpContext
RtcpSender::RtcpContext::RtcpContext(const RtcpSender::FeedbackState& feedback_state,
                                     const std::vector<uint16_t> nack_list,
                                     Timestamp now_time)
    : feedback_state(feedback_state),
      nack_list(std::move(nack_list)),
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

// FeedbackState
RtcpSender::FeedbackState::FeedbackState()
    : packets_sent(0),
      media_bytes_sent(0),
      send_bitrate(0),
      last_rr_ntp_secs(0),
      last_rr_ntp_frac(0),
      remote_sr(0){}

RtcpSender::FeedbackState::FeedbackState(const FeedbackState&) = default;

RtcpSender::FeedbackState::FeedbackState(FeedbackState&&) = default;

RtcpSender::FeedbackState::~FeedbackState() = default;
    
} // namespace naivertc
