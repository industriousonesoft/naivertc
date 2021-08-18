#include "rtc/rtp_rtcp/rtp_rtcp_impl.hpp"

namespace naivertc {

// Receiver
void RtpRtcpImpl::IncomingRtcpPacket(const uint8_t* incoming_packet, size_t packet_size) {
    task_queue_->Sync([this, incoming_packet, packet_size](){
        rtcp_receiver_.IncomingPacket(incoming_packet, packet_size);
    });
}

void RtpRtcpImpl::SetRemoteSsrc(uint32_t ssrc) {
    task_queue_->Sync([this, ssrc](){
        rtcp_receiver_.set_remote_ssrc(ssrc);
        rtcp_sender_.set_remote_ssrc(ssrc);
    });
}

void RtpRtcpImpl::SetLocalSsrc(uint32_t ssrc) {
    task_queue_->Sync([this, ssrc](){
        rtcp_receiver_.set_local_media_ssrc(ssrc);
        rtcp_sender_.set_ssrc(ssrc);
    });
}

// RTCP
int32_t RtpRtcpImpl::RemoteNTP(uint32_t* received_ntp_secs,
                               uint32_t* received_ntp_frac,
                               uint32_t* rtcp_arrival_time_secs,
                               uint32_t* rtcp_arrival_time_frac,
                               uint32_t* rtcp_timestamp) const {
    return -1;
}

int32_t RtpRtcpImpl::RTT(uint32_t remote_ssrc,
                         int64_t* rtt,
                         int64_t* avg_rtt,
                         int64_t* min_rtt,
                         int64_t* max_rtt) const {
    return -1;
}

int64_t RtpRtcpImpl::ExpectedRetransmissionTimeMs() const {
    return -1;
}

int32_t RtpRtcpImpl::SendRTCP(RtcpPacketType rtcp_packet_type) {
    return -1;
}

void RtpRtcpImpl::SetStorePacketsStatus(bool enable, uint16_t numberToStore) {

}

} // namespace naivertc