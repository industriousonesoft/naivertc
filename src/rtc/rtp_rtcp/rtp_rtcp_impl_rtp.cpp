#include "rtc/rtp_rtcp/rtp_rtcp_impl.hpp"

namespace naivertc {

void RtpRtcpImpl::SetMaxRtpPacketSize(size_t size) {
    
}

size_t RtpRtcpImpl::MaxRtpPacketSize() const {
    return 0;
}

void RegisterSendPayloadFrequency(int payload_type, int payload_frequency) {
    
}

int32_t RtpRtcpImpl::DeRegisterSendPayload(int8_t payload_type) {
    return -1;
}

// Returns current sending status.
bool RtpRtcpImpl::Sending() const {
    return false;
}

// Starts/Stops media packets. On by default.
void RtpRtcpImpl::SetSendingMediaStatus(bool sending) {

}

// Returns current media sending status.
bool RtpRtcpImpl::SendingMedia() const {
    return false;
}

// Record that a frame is about to be sent. Returns true on success, and false
// if the module isn't ready to send.
bool RtpRtcpImpl::OnSendingRtpFrame(uint32_t timestamp,
                                int64_t capture_time_ms,
                                int payload_type,
                                bool force_sender_report) {
    return false;
}

// Try to send the provided packet. Returns true if packet matches any of
// the SSRCs for this module (media/rtx/fec etc) and was forwarded to the
// transport.
bool RtpRtcpImpl::TrySendPacket(RtpPacketToSend* packet) {
    return false;
}

void RtpRtcpImpl::OnPacketsAcknowledged(std::vector<uint16_t> sequence_numbers) {

}

} // namespace naivertc