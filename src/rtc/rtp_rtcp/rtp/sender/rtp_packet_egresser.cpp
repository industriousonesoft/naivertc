#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr uint32_t kTimestampTicksPerMs = 90;
constexpr int kBitrateStatisticsWindowMs = 1000; // 1s
constexpr int kSendSideDelayWindowMs = 1000; // 1s
constexpr TimeDelta kUpdateInterval = TimeDelta::Millis(kBitrateStatisticsWindowMs);
} // namespace

RtpPacketEgresser::RtpPacketEgresser(const RtpConfiguration& config,
                                 RtpPacketSentHistory* const packet_history,
                                 FecGenerator* const fec_generator) 
        : is_audio_(config.audio),
          send_side_bwe_with_overhead_(config.send_side_bwe_with_overhead),
          clock_(config.clock),
          ssrc_(config.local_media_ssrc),
          rtx_ssrc_(config.rtx_send_ssrc),
          send_transport_(config.send_transport),
          packet_history_(packet_history),
          fec_generator_(fec_generator),
          sliding_sum_delay_ms_(0),
          accumulated_delay_ms_(0),
          max_delay_ms_(-1),
          worker_queue_(TaskQueueImpl::Current()),
          send_delay_observer_(config.send_side_delay_observer),
          send_bitrates_observer_(config.send_bitrates_observer),
          packet_send_stats_observer_(config.packet_send_stats_observer),
          stream_data_counters_observer_(config.stream_data_counters_observer) {
    // Init bitrate statistics
    // Audio or video media packet
    send_bitrate_stats_.emplace(config.audio ? RtpPacketType::AUDIO : RtpPacketType::VIDEO, kBitrateStatisticsWindowMs);
    // Retranmission packet
    send_bitrate_stats_.emplace(RtpPacketType::RETRANSMISSION, kBitrateStatisticsWindowMs);
    // Padding packet
    send_bitrate_stats_.emplace(RtpPacketType::PADDING, kBitrateStatisticsWindowMs);
    // FEC packet
    if (fec_generator_) {
        send_bitrate_stats_.emplace(RtpPacketType::FEC, kBitrateStatisticsWindowMs);
    }

    if (send_bitrates_observer_) {
        update_task_ = RepeatingTask::DelayedStart(clock_, worker_queue_, kUpdateInterval, [this](){
            PeriodicUpdate();
            return kUpdateInterval;
        });
    }
}
 
RtpPacketEgresser::~RtpPacketEgresser() {
    RTC_RUN_ON(&sequence_checker_);
    if (update_task_ && update_task_->Running()) {
        update_task_->Stop();
    }
}

 void RtpPacketEgresser::SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                                    const FecProtectionParams& key_params) {
    RTC_RUN_ON(&sequence_checker_);
    pending_fec_params_.emplace(delta_params, key_params);
}

void RtpPacketEgresser::SendPacket(RtpPacketToSend packet) {
    RTC_RUN_ON(&sequence_checker_);
    if (packet.empty()) {
        return;
    }
    if (!VerifySsrcs(packet)) {
        return;
    }

    if (packet.packet_type() == RtpPacketType::RETRANSMISSION && !packet.retransmitted_sequence_number().has_value()) {
        PLOG_WARNING << "Retransmission RTP packet can not send without retransmitted sequence number.";
        return;
    }

    // TODO: Update sequence number info map

    if (fec_generator_ && packet.fec_protection_need()) {
        std::optional<std::pair<FecProtectionParams, FecProtectionParams>> new_fec_params;
        new_fec_params.swap(pending_fec_params_);
        if (new_fec_params) {
            fec_generator_->SetProtectionParameters(/*delta_params=*/new_fec_params->first, 
                                                    /*key_params=*/new_fec_params->second);
        }
        // RED packet.
        if (packet.red_protection_need()) {
            assert(fec_generator_->fec_type() == FecGenerator::FecType::ULP_FEC);
            fec_generator_->PushMediaPacket(packet);
        } else {
            // If not RED encapsulated, we can just insert packet directly.
            fec_generator_->PushMediaPacket(packet);
        }
    }  

    const uint32_t packet_ssrc = packet.ssrc();
    const int64_t now_ms = clock_->now_ms();

    // Bug webrtc:7859. While FEC is invoked from rtp_sender_video, and not after
    // the pacer, these modifications of the header below are happening after the
    // FEC protection packets are calculated. This will corrupt recovered packets
    // at the same place. It's not an issue for extensions, which are present in
    // all the packets (their content just may be incorrect on recovered packets).
    // In case of VideoTimingExtension, since it's present not in every packet,
    // data after rtp header may be corrupted if these packets are protected by
    // the FEC.
    int64_t diff_ms = now_ms - packet.capture_time_ms();
    if (packet.HasExtension<rtp::TransmissionTimeOffset>()) {
        packet.SetExtension<rtp::TransmissionTimeOffset>(kTimestampTicksPerMs * diff_ms);
    }

    if (packet.HasExtension<rtp::AbsoluteSendTime>()) {
        packet.SetExtension<rtp::AbsoluteSendTime>(rtp::AbsoluteSendTime::MsTo24Bits(now_ms));
    }

    // TODO: Update VideoTimingExtension?

    auto packet_type = packet.packet_type();
    const bool is_media = packet_type == RtpPacketType::AUDIO ||
                          packet_type == RtpPacketType::VIDEO;
    
    auto transport_seq_num = packet.GetExtension<rtp::TransportSequenceNumber>();
    if (transport_seq_num) {
        OnPacketToSend(*transport_seq_num, packet);
    }

    if (packet_type != RtpPacketType::PADDING &&
        packet_type != RtpPacketType::RETRANSMISSION) {
        // No include Padding or Retransmission packet.
        UpdateDelayStatistics(packet.capture_time_ms(), now_ms, packet_ssrc);
    }

    // Put packet in retransmission history or update pending status even if
    // actual sending fails.
    if (is_media && packet.allow_retransmission()) {
        packet_history_->PutRtpPacket(packet, now_ms);
    } else if (packet.retransmitted_sequence_number()) {
        packet_history_->MarkPacketAsSent(*packet.retransmitted_sequence_number());
    }

    // Send statistics
    SendStats send_stats(packet.ssrc(), packet.size(), packet_type, RtpPacketCounter(packet));

    PacketOptions options;
    // Set recommended medium-priority DSCP value
    // See https://datatracker.ietf.org/doc/html/draft-ietf-tsvwg-rtcweb-qos-18
    if (is_audio_) {
        options.kind = PacketKind::AUDIO;
        options.dscp = DSCP::DSCP_EF; // EF: Expedited Forwarding
    } else {
        options.kind = PacketKind::VIDEO;
        options.dscp = DSCP::DSCP_AF42; // AF42: Assured Forwarding class 4, medium drop probability
    }
    const bool send_success = SendPacketToNetwork(std::move(packet), std::move(options));

    // NOTE: The `packet` was moved to other, DO NOT use it any more.

    if (send_success) {
        // |media_has_been_sent_| is used by RTPSender to figure out if it can send
        // padding in the absence of transport-cc or abs-send-time.
        // In those cases media must be sent first to set a reference timestamp.
        media_has_been_sent_ = true;

        // TODO: Add support for FEC protecting all header extensions, 
        // add media packet to generator here instead.
        
        UpdateSentStatistics(now_ms, std::move(send_stats));
    }
}

std::vector<RtpPacketToSend> RtpPacketEgresser::FetchFecPackets() const {
    RTC_RUN_ON(&sequence_checker_);
    if (fec_generator_) {
        return fec_generator_->PopFecPackets();
    }
    return {};
}

DataRate RtpPacketEgresser::GetTotalSendBitrate() {
    RTC_RUN_ON(&sequence_checker_);
    return CalcTotalSendBitrate(clock_->now_ms());
}

RtpStreamDataCounters RtpPacketEgresser::GetRtpStreamDataCounter() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtp_send_counter_;
}
    
RtpStreamDataCounters RtpPacketEgresser::GetRtxStreamDataCounter() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtx_send_counter_;
}

// Private methods
DataRate RtpPacketEgresser::GetSendBitrate(RtpPacketType packet_type) {
    auto it = send_bitrate_stats_.find(packet_type);
    if (it != send_bitrate_stats_.end()) {
        return it->second.Rate(clock_->now_ms()).value_or(DataRate::Zero());
    }
    return DataRate::Zero();
}

DataRate RtpPacketEgresser::CalcTotalSendBitrate(const int64_t now_ms) {
    DataRate send_bitrate = DataRate::Zero();
    for (auto& kv : send_bitrate_stats_) {
        send_bitrate += kv.second.Rate(now_ms).value_or(DataRate::Zero());
    }
    return send_bitrate;
}

bool RtpPacketEgresser::SendPacketToNetwork(RtpPacketToSend packet, PacketOptions options) {
    if (send_transport_) {
        if (!send_transport_->SendRtpPacket(std::move(packet), std::move(options))) {
            PLOG_WARNING << "Transport faild to send packet.";
            return false;
        }
     }
    return true;
}

bool RtpPacketEgresser::VerifySsrcs(const RtpPacketToSend& packet) {
    switch (packet.packet_type())
    {
    case RtpPacketType::AUDIO:
    case RtpPacketType::VIDEO:
        return packet.ssrc() == ssrc_;
    case RtpPacketType::RETRANSMISSION:
    case RtpPacketType::PADDING:
        // Both padding and retransmission must be on either the media
        // or the RTX stream.
        return packet.ssrc() == ssrc_ || packet.ssrc() == rtx_ssrc_;
    case RtpPacketType::FEC:
        // FlexFEC is on separate SSRC, ULPFEC uses media SSRC.
        return packet.ssrc() == ssrc_ || packet.ssrc() == fec_generator_->fec_ssrc();
    }
    return false;
}

void RtpPacketEgresser::OnPacketToSend(uint16_t packet_id, 
                                       const RtpPacketToSend& packet) {
    if (packet_send_stats_observer_) {
        const size_t packet_size = send_side_bwe_with_overhead_ ? packet.size() 
                                                                : packet.payload_size() + packet.padding_size();
        
        RtpPacketSendStats send_stats;
        send_stats.packet_id = packet_id;
        send_stats.rtp_timestamp = packet.timestamp();
        send_stats.packet_size = packet_size;
        send_stats.packet_type = packet.packet_type();
        send_stats.ssrc = packet.ssrc();
        send_stats.seq_num = packet.sequence_number();

        switch (packet.packet_type())
        {
        case RtpPacketType::AUDIO:
        case RtpPacketType::VIDEO:
            break;
        case RtpPacketType::RETRANSMISSION:
            // For retransmissions, we're want to remove the original media packet
            // if the rentrasmit arrives, so populate that in the packet send statistics.
            send_stats.retransmitted_seq_num = packet.retransmitted_sequence_number();
            break;
        case RtpPacketType::PADDING:
        case RtpPacketType::FEC:
            // We're not interested in feedback about these packets being received
            // or lost.
            break;
        default:
            break;
        }

        packet_send_stats_observer_->OnPacketToSend(send_stats);
    }
}

void RtpPacketEgresser::UpdateSentStatistics(const int64_t now_ms, SendStats send_stats) {
    // NOTE: We will send the retransmitted packets and padding packets by RTX stream, but:
    // 1) the RTX stream can send either the retransmitted packets or the padding packets
    // 2) the retransmitted packets can be sent by either the meida stream or the RTX stream
    // see https://blog.csdn.net/sonysuqin/article/details/82021185
    auto send_counters = send_stats.ssrc == rtx_ssrc_ ? &rtx_send_counter_ : &rtp_send_counter_;

    if (send_counters->first_packet_time_ms == -1) {
        send_counters->first_packet_time_ms = now_ms;
    }

    // FEC packet
    if (send_stats.packet_type == RtpPacketType::FEC) {
        send_counters->fec += send_stats.packet_counter;
    // Retransmittion packet
    } else if (send_stats.packet_type == RtpPacketType::RETRANSMISSION) {
        send_counters->retransmitted += send_stats.packet_counter;
    }
    send_counters->transmitted += send_stats.packet_counter;

    if (stream_data_counters_observer_) {
        stream_data_counters_observer_->OnStreamDataCountersUpdated(*send_counters, send_stats.ssrc);
    }
  
    // Update send bitrate
    // NOTE: "operator[]" is not working here, since "BitRateStatistics" has no parameterless constructor,
    // which is required to create a new one if no element found for the key.
    // So we using "at" instead, since it access specified element with bounds checking.
    send_bitrate_stats_.at(send_stats.packet_type).Update(send_stats.packet_size, now_ms);
}

void RtpPacketEgresser::UpdateDelayStatistics(int64_t capture_time_ms, 
                                              int64_t now_ms, 
                                              uint32_t ssrc) {
    if (!send_delay_observer_ || capture_time_ms <= 0) {
        return;
    }

    // Remove elements older than kSendSideDelayWindowMs
    auto lower_bound = send_delays_.lower_bound(now_ms - kSendSideDelayWindowMs);
    for (auto it = send_delays_.begin(); it != lower_bound; ++it) {
        if (max_delay_ms_ == it->second) {
            max_delay_ms_ = -1;
        }
        sliding_sum_delay_ms_ -= it->second;
    }
    send_delays_.erase(send_delays_.begin(), lower_bound);
    // The previous max was removed, we need to recompute.
    if (max_delay_ms_ < 0) {
        max_delay_ms_ = CalcMaxDelay();
    }
    int64_t new_delay_ms = now_ms - capture_time_ms;
    assert(new_delay_ms > 0);
    auto [it, inserted] = send_delays_.insert({now_ms, new_delay_ms});
    if (!inserted) {
        // Keep the most resent one if we have multiple delay meansurements
        // during the same time.
        int previous_delay_ms = it->second;
        sliding_sum_delay_ms_ -= previous_delay_ms;
        it->second = new_delay_ms;
    }
    if (max_delay_ms_ < new_delay_ms) {
        max_delay_ms_ = new_delay_ms;
    }
    sliding_sum_delay_ms_ += new_delay_ms;
    accumulated_delay_ms_ += new_delay_ms;
    size_t num_delays = send_delays_.size();
    int64_t avg_delay_ms = (sliding_sum_delay_ms_ + num_delays / 2) / num_delays;
    assert(avg_delay_ms > 0);
    send_delay_observer_->OnSendDelayUpdated(avg_delay_ms, max_delay_ms_, accumulated_delay_ms_, ssrc);
}

int RtpPacketEgresser::CalcMaxDelay() const {
    int max_delay_ms = -1;
    for (const auto& kv : send_delays_) {
        if (kv.second > max_delay_ms) {
            max_delay_ms = kv.second;
        }
    }
    return max_delay_ms;
}

void RtpPacketEgresser::PeriodicUpdate() {
    if (send_bitrates_observer_) {
        const int64_t now_ms = clock_->now_ms();
        send_bitrates_observer_->OnSendBitratesUpdated(CalcTotalSendBitrate(now_ms).bps(),
                                                       GetSendBitrate(RtpPacketType::RETRANSMISSION).bps(), 
                                                       ssrc_);
    }
}
    
} // namespace naivertc
