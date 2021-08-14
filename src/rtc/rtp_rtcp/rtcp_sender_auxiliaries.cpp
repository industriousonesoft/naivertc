#include "rtc/rtp_rtcp/rtcp_sender.hpp"

namespace naivertc {

// RtcpContext
RtcpSender::RtcpContext::RtcpContext(const RtcpSender::FeedbackState& feedback_state,
                                    int32_t nack_size,
                                    const uint16_t* nack_list,
                                    Timestamp now)
    : feedback_state_(feedback_state),
        nack_size_(nack_size),
        nack_list_(nack_list),
        now_(now) {}

// PacketSender
RtcpSender::PacketSender::PacketSender(RtcpPacket::PacketReadyCallback callback,
                                       size_t max_packet_size)
    : callback_(callback), 
    max_packet_size_(max_packet_size) {
    assert(max_packet_size <= kIpPacketSize);
}

RtcpSender::PacketSender::~PacketSender() {}

void RtcpSender::PacketSender::AppendPacket(const RtcpPacket& packet) {
    packet.PackInto(buffer_, &index_, max_packet_size_, callback_);
}

void RtcpSender::PacketSender::Send() {
    if (index_ > 0) {
        callback_(BinaryBuffer(buffer_, &buffer_[index_]));
        index_ = 0;
    }
}

// FeedbackState
RtcpSender::FeedbackState::FeedbackState()
    : packets_sent(0),
      media_bytes_sent(0),
      send_bitrate(0),
      last_rr_ntp_secs(0),
      last_rr_ntp_frac(0),
      remote_sr(0),
      receiver(nullptr) {}

RtcpSender::FeedbackState::FeedbackState(const FeedbackState&) = default;

RtcpSender::FeedbackState::FeedbackState(FeedbackState&&) = default;

RtcpSender::FeedbackState::~FeedbackState() = default;
    
} // namespace naivertc
