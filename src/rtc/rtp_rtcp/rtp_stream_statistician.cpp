#include "rtc/rtp_rtcp/rtp_stream_statistician.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr int64_t kStatisticsTimeoutMs = 8000; // 8s
constexpr int64_t kStatisticsProcessIntervalMs = 1000; // 1s

}  // namespace

RtpStreamStatistician::RtpStreamStatistician(uint32_t ssrc, Clock* clock, int max_reordering_threshold) 
    : ssrc_(ssrc),
      clock_(clock),
      delta_internal_unix_epoch_ms_(/*time based on Unix epoch*/(clock_->now_ntp_time_ms() - kNtpJan1970Ms) - clock_->now_ms()),
      max_reordering_threshold_(max_reordering_threshold),
      enable_retransmit_detection_(false),
      cumulative_loss_is_capped_(false),
      jitter_q4_(0),
      cumulative_loss_(0),
      cumulative_loss_rtcp_offset_(0),
      last_receive_time_ms_(0),
      last_packet_timestamp_(0),
      first_received_seq_num_(-1),
      last_received_seq_num_(-1),
      last_report_cumulative_loss_(0),
      last_report_max_seq_num_(-1) {}
    
RtpStreamStatistician::~RtpStreamStatistician() = default;

void RtpStreamStatistician::set_max_reordering_threshold(int threshold) {
    max_reordering_threshold_ = threshold;
}
    
void RtpStreamStatistician::set_enable_retransmit_detection(bool enable) {
    enable_retransmit_detection_ = enable;
}

void RtpStreamStatistician::OnRtpPacket(const RtpPacketReceived& packet) {
    assert(ssrc_ == packet.ssrc());
    int64_t now_ms = clock_->now_ms();

    receive_counters_.last_packet_received_time_ms = now_ms;
    receive_counters_.transmitted.AddPacket(packet);
    --cumulative_loss_;

    int64_t unwrapped_seq_num = seq_unwrapper_.Unwrap(packet.sequence_number(), /*update_last=*/true);

    if (!HasReceivedRtpPacket()) {
        // The first packet.
        first_received_seq_num_ = unwrapped_seq_num;
        last_received_seq_num_ = unwrapped_seq_num - 1;
        last_report_max_seq_num_ = last_received_seq_num_;
        receive_counters_.first_packet_time_ms = now_ms;
    } else if (IsOutOfOrderPacket(packet, unwrapped_seq_num, now_ms)) {
        // Ignore the out-of-order packet for statistics.
        return;
    }

    // The incoming packet is in order.
    cumulative_loss_ += unwrapped_seq_num - last_received_seq_num_;
    last_received_seq_num_ = unwrapped_seq_num;

    // If a new timestamp and more than one in-order 
    // packet received, calculate the new jitter.
    if (packet.timestamp() != last_packet_timestamp_ && 
        (receive_counters_.transmitted.num_packets - receive_counters_.retransmitted.num_packets) > 1) {
        UpdateJitter(packet, now_ms);
    }
    last_packet_timestamp_ = packet.timestamp();
    last_receive_time_ms_ = now_ms;
}

std::optional<rtcp::ReportBlock> RtpStreamStatistician::GetReportBlock() {
    int64_t now_ms = clock_->now_ms();
    if (now_ms - last_receive_time_ms_ >= kStatisticsTimeoutMs) {
        // The statistician is not active any more.
        return std::nullopt;
    }

    if (!HasReceivedRtpPacket()) {
        return std::nullopt;
    }

    rtcp::ReportBlock report_block;
    report_block.set_media_ssrc(ssrc_);
    
    // Calculate fraction loss.
    // The received packets since last report.
    int64_t received_packets_since_last = last_received_seq_num_ - last_report_max_seq_num_;
    assert(received_packets_since_last >= 0);

    // The lost packets since last report.
    int32_t lost_packets_since_last = cumulative_loss_ - last_report_cumulative_loss_;
    if (received_packets_since_last > 0 && lost_packets_since_last > 0) {
        // Scale to 0 ~ 255, where 255 means 100% loss.
        report_block.set_fraction_lost(255 * lost_packets_since_last / received_packets_since_last);
    }

    int packet_lost = cumulative_loss_ + cumulative_loss_rtcp_offset_;
    if (packet_lost < 0) {
        // Clamp to zero in case of the senders will misbehave with 
        // a negative cumulative loss.
        packet_lost = 0;
        cumulative_loss_rtcp_offset_ = -cumulative_loss_;
    }
    
    // The max value represented in 24 bits.
    const uint32_t kPacketLostCappedValue = 0x7fffff;
    if (packet_lost > kPacketLostCappedValue) {
        if (!cumulative_loss_is_capped_) {
            cumulative_loss_is_capped_ = true;
            PLOG_WARNING << "Cumulative loss reached the maximum value for ssrc = " << ssrc_;
        }
        packet_lost = kPacketLostCappedValue;
    }
    report_block.set_cumulative_packet_lost(packet_lost);
    // TODO: Is there a auto cast happened between int64_t and uint32?
    report_block.set_extended_highest_sequence_num(last_received_seq_num_);
    report_block.set_jitter(jitter_q4_ >> 4);

    last_report_cumulative_loss_ = cumulative_loss_;
    last_report_max_seq_num_ = last_received_seq_num_;

    return report_block;
}

RtpReceiveStats RtpStreamStatistician::GetStates() const {
    RtpReceiveStats stats;
    stats.packets_lost = cumulative_loss_;
    stats.jitter = jitter_q4_ >> 4;
    if (receive_counters_.last_packet_received_time_ms.has_value()) {
        // TODO: how to understand `delta_internal_unix_epoch_ms_`?
        // TODO: Why we need to add `delta_internal_unix_epoch_ms_`?
        stats.last_packet_received_time_ms = *receive_counters_.last_packet_received_time_ms + delta_internal_unix_epoch_ms_;
    }
    stats.packet_counter = receive_counters_.transmitted;
    return stats;
}

std::optional<int> RtpStreamStatistician::GetFractionLostInPercent() const {
    if (!HasReceivedRtpPacket()) {
        return std::nullopt;
    }
    int64_t expected_packets = 1 + last_received_seq_num_ - first_received_seq_num_;
    if (expected_packets <= 0) {
        return std::nullopt;
    }
    if (cumulative_loss_ <= 0) {
        return 0;
    }
    return 100 * static_cast<int64_t>(cumulative_loss_) / expected_packets;
}

// Private methods
bool RtpStreamStatistician::HasReceivedRtpPacket() const {
    return first_received_seq_num_ >= 0;
}

bool RtpStreamStatistician::IsRetransmitedPacket(const RtpPacketReceived& packet, 
                                                 int64_t receive_time_ms) const {
    uint32_t frequency_khz = packet.payload_type_frequency() / 1000;
    assert(frequency_khz > 0);

    int64_t receive_time_diff_ms = receive_time_ms - last_receive_time_ms_;

    // Diff in timestamp since last received in order.
    uint32_t send_timestamp_diff = packet.timestamp() - last_packet_timestamp_;
    uint32_t send_time_diff_ms = send_timestamp_diff / frequency_khz;

    // Jitter standard deviation in samples.
    float jitter_std = std::sqrt(static_cast<float>(jitter_q4_ >> 4));

    // 2 times the standard deviation => 95% confidence.
    // And transform to milliseconds by dividing by the frequency in khz.
    int64_t max_delay_ms = static_cast<int64_t>((2 * jitter_std) / frequency_khz);

    // The minimum value of `max_delay_ms` is 1.
    if (max_delay_ms <= 0) {
        max_delay_ms = 1;
    }

    return receive_time_diff_ms > send_time_diff_ms + max_delay_ms;
}

void RtpStreamStatistician::UpdateJitter(const RtpPacketReceived& packet, int64_t receive_time_ms) {
    int64_t receive_diff_ms = receive_time_ms - last_receive_time_ms_;
    assert(receive_diff_ms > 0);

    // Receive diff in samples.
    // See https://datatracker.ietf.org/doc/html/rfc3550, (`interarrival jitter` in ReportBlock packet)
    // The difference in the `relative transit time` for two packtes.
    // D(i,j) = (Rj - Ri) - (Sj - Si) = (Rj - Sj) - (Ri - Si)
    uint32_t receive_timestamp_diff = static_cast<uint32_t>(receive_diff_ms * packet.payload_type_frequency() / 1000);
    uint32_t send_timestamp_diff = packet.timestamp() - last_packet_timestamp_;
    int32_t transit_timestamp_diff = receive_timestamp_diff - send_timestamp_diff;

    transit_timestamp_diff = std::abs(transit_timestamp_diff);

    // Use 5 seconds video frequency as the threshold.
    // NOTE: In case of a crazy jumps happens.
    const int32_t JitterDiffThreshold = 450000; // 5 * 90000
    if (transit_timestamp_diff < JitterDiffThreshold) {
        // The interarrival jitter J is defined to be the mean deviation 
        // (smoothed absolute value) of the difference D in packet spacing 
        // at the receiver compared to the sender for a pair of packets.
        // J(i) = J(i-1) + (|D(i-1,i)| - J(i-1))/16
        // NOTE: We calculate in Q4 to avoid using float.
        int32_t jitter_diff_q4 = (transit_timestamp_diff << 4) - jitter_q4_;
        // Smoothing filter
        jitter_q4_ += ((jitter_diff_q4 + /*round up*/8) >> 4);
    }
}

bool RtpStreamStatistician::IsOutOfOrderPacket(const RtpPacketReceived& packet, 
                                               int64_t unwrapped_seq_num, 
                                               int64_t receive_time_ms) {
    // Check if `packet` is second packet of a restarted stream.
    if (received_seq_out_of_order_.has_value()) {
        // Count the previous packet as a received packet.
        --cumulative_loss_;

        uint16_t expected_seq_num = *received_seq_out_of_order_ + 1;
        received_seq_out_of_order_.reset();

        // The incoming packet is the second packet of a restarted stream.
        if (packet.sequence_number() == expected_seq_num) {
            // Ignore the sequence number gap caused by stream restarted for packet loss
            // calculation, by setting `last_received_seq_num_` to the sequence number just
            // before the out-of-order sequence number.
            //
            // Fraction loss for the next report my get a bit off, since we don't update
            // `last_report_max_seq_num_` and `last_report_cumulative_loss_` in a consistent
            // way.
            last_received_seq_num_  = unwrapped_seq_num - 2;
            last_report_max_seq_num_ = last_received_seq_num_;
            return false;
        }
    }

    if (std::abs(unwrapped_seq_num - last_received_seq_num_) > max_reordering_threshold_) {
        // The sequence number gap seems too larger, we wait until the next
        // packet to check for a stream restart.
        received_seq_out_of_order_ = packet.sequence_number();

        // Postpone counting this packet as a received packet until we know how to
        // update `last_received_seq_num_`, otherwise we temporarily decrement `cumulative_loss_`.
        ++cumulative_loss_;
        return true;
    }

    // The incoming packet is in order.
    if (unwrapped_seq_num > last_received_seq_num_) {
        return false;
    }

    // The incoming packet is out of order.

    // The out-of-order packet may be a retransmited packet.
    if (enable_retransmit_detection_ && IsRetransmitedPacket(packet, receive_time_ms)) {
        receive_counters_.retransmitted.AddPacket(packet);
    }

    return true;
} 
    
} // namespace naivertc
