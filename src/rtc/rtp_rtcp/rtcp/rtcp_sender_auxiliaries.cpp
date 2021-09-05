#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"

namespace naivertc {

RtcpSender::Configuration RtcpSender::Configuration::FromRtpRtcpConfiguration(const RtpRtcpInterface::Configuration& config) {
    RtcpSender::Configuration sender_config;

    sender_config.audio = config.audio;
    sender_config.local_media_ssrc = config.local_media_ssrc;
    if (config.rtcp_report_interval_ms > 0) {
        sender_config.rtcp_report_interval = TimeDelta::Millis(config.rtcp_report_interval_ms);
    }
    return sender_config;
}

// RtcpContext
RtcpSender::RtcpContext::RtcpContext(const RtcpSender::FeedbackState& feedback_state,
                                     const std::vector<uint16_t> nack_list,
                                    Timestamp now)
    : feedback_state_(feedback_state),
      nack_list_(std::move(nack_list)),
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
      remote_sr(0){}

RtcpSender::FeedbackState::FeedbackState(const FeedbackState&) = default;

RtcpSender::FeedbackState::FeedbackState(FeedbackState&&) = default;

RtcpSender::FeedbackState::~FeedbackState() = default;
    
} // namespace naivertc
