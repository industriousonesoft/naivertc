#include "rtc/congestion_control/components/network_transport_statistician.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kPacketFeedbackHistoryWindow = TimeDelta::Seconds(60); // 1 minute
    
} // namespace

NetworkTransportStatistician::NetworkTransportStatistician() 
    : last_acked_packet_id_(-1),
      inflight_bytes_(0),
      last_send_time_(Timestamp::MinusInfinity()),
      last_untracked_send_time_(Timestamp::MinusInfinity()),
      pending_untracked_bytes_(0),
      last_feedback_recv_time_(Timestamp::MinusInfinity()),
      last_timestamp_(TimeDelta::MinusInfinity()) {
    sequence_checker_.Detach();
}
    
NetworkTransportStatistician::~NetworkTransportStatistician() = default;

size_t NetworkTransportStatistician::GetInFlightBytes() const {
    RTC_RUN_ON(&sequence_checker_);
    return inflight_bytes_;
}

void NetworkTransportStatistician::AddPacket(const RtpPacketSendInfo& packet_info, 
                                             size_t overhead_bytes, 
                                             Timestamp receive_time) {
    RTC_RUN_ON(&sequence_checker_);
    PacketFeedback feedback;
    feedback.creation_time = receive_time;
    feedback.sent.packet_id = seq_num_unwrapper_.Unwrap(packet_info.packet_id);
    feedback.sent.size = packet_info.packet_size + overhead_bytes;
    feedback.sent.is_audio = packet_info.packet_type == RtpPacketType::AUDIO;
    // TODO: Add PacingInfo

    // Erases the old items in |packet_fb_history_|.
    while (!packet_fb_history_.empty() && 
           receive_time - packet_fb_history_.begin()->second.creation_time > kPacketFeedbackHistoryWindow) {
        // Igonring the in-flight packet 
        if (IsInFlight(packet_fb_history_.begin()->second.sent)) {
            inflight_bytes_ -= packet_fb_history_.begin()->second.sent.size;
        }
        packet_fb_history_.erase(packet_fb_history_.begin());
    }
    // Inserts the new feedback.
    packet_fb_history_.insert({feedback.sent.packet_id, feedback});
}

std::optional<SentPacket> NetworkTransportStatistician::ProcessSentPacket(const RtpSentPacket& sent_packet) {
    RTC_RUN_ON(&sequence_checker_);
    if (sent_packet.packet_id) {
        int64_t packet_id = seq_num_unwrapper_.Unwrap(*sent_packet.packet_id);
        auto it = packet_fb_history_.find(packet_id);
        if (it != packet_fb_history_.end()) {
            bool retransmit = it->second.sent.send_time.IsFinite();
            it->second.sent.send_time = sent_packet.send_time;
            last_send_time_ = std::max(last_send_time_, sent_packet.send_time);
            if (pending_untracked_bytes_ > 0) {
                if (sent_packet.send_time < last_untracked_send_time_) {
                    auto diff = last_untracked_send_time_ - sent_packet.send_time;
                    PLOG_WARNING << "appending acknowledged data for out of order packet. (Diff: "
                                 << diff.ms() << " ms.)";
                }
                // The bytes untracked by transport feedback but almost acknowledged by remote,
                // like the audio packets which might be too small to drop.
                it->second.sent.prior_unacked_bytes += pending_untracked_bytes_;
                pending_untracked_bytes_ = 0;
            }
            if (!retransmit) {
                if (IsInFlight(it->second.sent)) {
                    inflight_bytes_ += it->second.sent.size;
                }
                it->second.sent.bytes_in_flight = inflight_bytes_;
                return it->second.sent;
            }
        }
    } else if (sent_packet.included_in_allocation) {
        if (sent_packet.send_time < last_send_time_) {
            PLOG_WARNING << "ignoring untracked data for out of order packet.";
        }
        // FIXME: Do we need to account the overhead of transport into the sent size?
        pending_untracked_bytes_ += sent_packet.size;
        last_untracked_send_time_ = std::max(last_untracked_send_time_, sent_packet.send_time);
    }
    return std::nullopt;
}

std::optional<TransportPacketsFeedback> 
NetworkTransportStatistician::ProcessTransportFeedback(const rtcp::TransportFeedback& feedback,
                                                       Timestamp receive_time) {
    RTC_RUN_ON(&sequence_checker_);
    if (feedback.GetPacketStatusCount() == 0) {
        PLOG_WARNING << "Received a empty transport feedback packet.";
        return std::nullopt;
    }

    Timestamp last_acked_recv_time = Timestamp::MinusInfinity();

    TransportPacketsFeedback report;
    report.receive_time = receive_time;
    report.prior_in_flight = inflight_bytes_;
    if (ParsePacketFeedbacks(feedback, receive_time, report)) {
        return std::nullopt;
    }

    auto it = packet_fb_history_.find(last_acked_packet_id_);
    if (it != packet_fb_history_.end()) {
        report.first_unacked_send_time = it->second.sent.send_time;
    }
    report.bytes_in_flight = inflight_bytes_;
    return report;
}

// Private methods
bool NetworkTransportStatistician::IsInFlight(const SentPacket& packet) {
    return packet.packet_id > last_acked_packet_id_;
}

bool NetworkTransportStatistician::ParsePacketFeedbacks(const rtcp::TransportFeedback& feedback,
                                                        Timestamp receive_time,
                                                        TransportPacketsFeedback& report) {
    if (last_timestamp_.IsInfinite()) {
        last_feedback_recv_time_ = receive_time;
    } else {
        const TimeDelta delta = feedback.GetBaseDelta(last_timestamp_);
        if (last_feedback_recv_time_ + delta > Timestamp::Zero()) {
            last_feedback_recv_time_ += delta;
        } else {
            PLOG_WARNING << "Received a unexpected feedback timestamp.";
            last_feedback_recv_time_ = receive_time;
        }
    }
    last_timestamp_ = feedback.GetBaseTime();

    report.packet_feedbacks.reserve(feedback.GetPacketStatusCount());

    size_t num_droping_packets = 0;
    TimeDelta packet_offset = TimeDelta::Zero();
    for (const auto& packet : feedback.GetAllPackets()) {
        int64_t packet_id = seq_num_unwrapper_.Unwrap(packet.sequence_number());

        // Updates the last acked packet id.
        if (packet_id > last_acked_packet_id_) {
            // Update the inflight bytes.
            for (auto it = packet_fb_history_.upper_bound(last_acked_packet_id_);
                 it != packet_fb_history_.upper_bound(packet_id); ++it) {
                inflight_bytes_ -= it->second.sent.size;
            }
            last_acked_packet_id_ = packet_id;
        }

        auto it = packet_fb_history_.find(packet_id);
        // If the feedback arrived too late, droping it.
        if (it == packet_fb_history_.end()) {
            ++num_droping_packets;
            continue;
        }

        if (it->second.sent.send_time.IsInfinite()) {
            PLOG_WARNING << "Received feedback before packet was indicated as sent.";
            continue;
        }

        PacketResult feedback;
        feedback.sent_packet = it->second.sent;
        // The packet has been received by the remote peer.
        if (packet.received()) {
            packet_offset += packet.delta();
            feedback.recv_time = last_feedback_recv_time_ + packet_offset.RoundDownTo(TimeDelta::Millis(1));
            // NOTE: The lost packets are not erased from history as they might be
            // reported as received by a later feedback.
            packet_fb_history_.erase(it);
            report.last_acked_recv_time = std::max(report.last_acked_recv_time, feedback.recv_time);
        }

        report.packet_feedbacks.push_back(std::move(feedback));
    }

    if (num_droping_packets > 0) {
        PLOG_WARNING << "Failed to lookup send time for " << num_droping_packets
                     << " packet" << (num_droping_packets > 1 ? "s" : "")
                     << ". Send time history too small?";
    }

    return !report.packet_feedbacks.empty();
}

} // namespace naivertc
