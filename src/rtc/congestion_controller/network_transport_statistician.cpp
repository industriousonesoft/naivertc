#include "rtc/congestion_controller/network_transport_statistician.hpp"
#include "rtc/base/time/clock.hpp"

namespace naivertc {
namespace {

constexpr TimeDelta kPacketFeedbackHistoryWindow = TimeDelta::Seconds(60); // 1 minute
    
} // namespace


NetworkTransportStatistician::NetworkTransportStatistician(Clock* clock) 
    : clock_(clock),
      last_acked_packet_id_(-1) {}
    
NetworkTransportStatistician::~NetworkTransportStatistician() = default;

void NetworkTransportStatistician::OnSendFeedback(const RtpSendFeedback& send_feedback, size_t overhead_bytes) {
    PacketFeedback feedback;
    auto now = clock_->CurrentTime();
    feedback.creation_time = now;
    feedback.sent.packet_id = seq_num_unwrapper_.Unwrap(send_feedback.packet_id);
    feedback.sent.size = send_feedback.packet_size + overhead_bytes;
    feedback.sent.is_audio = send_feedback.packet_type == RtpPacketType::AUDIO;
    // TODO: Add PacingInfo

    // Erases the old items in |packet_fb_history_|.
    while (!packet_fb_history_.empty() && 
           now - packet_fb_history_.begin()->second.creation_time > kPacketFeedbackHistoryWindow) {
        // Igonring the in-flight packet 
        if (IsInFlight(packet_fb_history_.begin()->second.sent)) {
            inflight_bytes_ -= packet_fb_history_.begin()->second.sent.size;
        }
        packet_fb_history_.erase(packet_fb_history_.begin());
    }
    // Inserts the new feedback.
    packet_fb_history_.insert({feedback.sent.packet_id, feedback});
}

// Private methods
bool NetworkTransportStatistician::IsInFlight(const SentPacket& packet) {
    return packet.packet_id > last_acked_packet_id_;
}

} // namespace naivertc
