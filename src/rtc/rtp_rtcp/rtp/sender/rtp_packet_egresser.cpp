#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr uint32_t kTimestampTicksPerMs = 90;

constexpr int kSendSideDelayWindowMs = 1000; // 1s
constexpr auto kUpdateInterval = TimeDelta::Millis(BitrateStatistics::kDefauleWindowSizeMs);

} // namespace

RtpPacketEgresser::RtpPacketEgresser(const RtpConfiguration& config,
                                     SequenceNumberAssigner* seq_num_assigner,
                                     RtpPacketHistory* const packet_history) 
        : is_audio_(config.audio),
          send_side_bwe_with_overhead_(config.send_side_bwe_with_overhead),
          clock_(config.clock),
          ssrc_(config.local_media_ssrc),
          rtx_ssrc_(config.rtx_send_ssrc),
          flex_fec_ssrc_(config.fec_generator ? config.fec_generator->fec_ssrc() : std::nullopt),
          send_transport_(config.send_transport),
          packet_history_(packet_history),
          fec_generator_(config.fec_generator),
          seq_num_assigner_(seq_num_assigner),
          max_delay_it_(send_delays_.end()),
          worker_queue_(TaskQueueImpl::Current()),
          send_delay_observer_(config.send_delay_observer),
          send_packet_observer_(config.send_packet_observer),
          send_bitrates_observer_(config.send_bitrates_observer),
          transport_feedback_observer_(config.transport_feedback_observer),
          stream_data_counters_observer_(config.stream_data_counters_observer) {
    assert(worker_queue_ != nullptr);
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

uint32_t RtpPacketEgresser::ssrc() const { 
    RTC_RUN_ON(&sequence_checker_);
    return ssrc_; 
}
    
std::optional<uint32_t> RtpPacketEgresser::rtx_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtx_ssrc_; 
}
   
std::optional<uint32_t> RtpPacketEgresser::flex_fec_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return flex_fec_ssrc_;
}

bool RtpPacketEgresser::media_has_been_sent() const {
    RTC_RUN_ON(&sequence_checker_);
    return media_has_been_sent_;
}

void RtpPacketEgresser::SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                                   const FecProtectionParams& key_params) {
    RTC_RUN_ON(&sequence_checker_);
    pending_fec_params_.emplace(delta_params, key_params);
}

bool RtpPacketEgresser::SendPacket(RtpPacketToSend packet,
                                   std::optional<const PacedPacketInfo> pacing_info) {
    RTC_RUN_ON(&sequence_checker_);
    if (packet.empty()) {
        return false;
    }
    if (!VerifySsrcs(packet)) {
        return false;
    }
    if (packet.packet_type() == RtpPacketType::RETRANSMISSION && 
        !packet.retransmitted_sequence_number().has_value()) {
        PLOG_WARNING << "Retransmission RTP packet can not send without retransmitted sequence number.";
        return false;
    }

    auto packet_id = PrepareForSend(packet);

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
    int64_t send_delay_ms = now_ms - packet.capture_time_ms();
    if (packet.HasExtension<rtp::TransmissionTimeOffset>()) {
        packet.SetExtension<rtp::TransmissionTimeOffset>(kTimestampTicksPerMs * send_delay_ms);
    }

    if (packet.HasExtension<rtp::AbsoluteSendTime>()) {
        packet.SetExtension<rtp::AbsoluteSendTime>(rtp::AbsoluteSendTime::MsTo24Bits(now_ms));
    }

    // TODO: Update VideoTimingExtension?

    auto packet_type = packet.packet_type();
    const bool is_media = packet_type == RtpPacketType::AUDIO ||
                          packet_type == RtpPacketType::VIDEO;
    
    PacketOptions options(is_audio_ ? PacketKind::AUDIO : PacketKind::VIDEO);

    // Report transport feedback.
    // // auto packet_id = packet.GetExtension<rtp::TransportSequenceNumber>();
    if (packet_id) {
        PLOG_VERBOSE_IF(false) << "Will send packet with transport sequence number: " << *packet_id;
        options.packet_id = packet_id;
        AddPacketToTransportFeedback(*packet_id, packet, pacing_info);
    }

    if (packet_type != RtpPacketType::PADDING &&
        packet_type != RtpPacketType::RETRANSMISSION) {
        // No include Padding or Retransmission packet.
        UpdateDelayStatistics(send_delay_ms, now_ms, packet_ssrc);
        if (packet_id && send_packet_observer_) {
            send_packet_observer_->OnSendPacket(*packet_id, packet.capture_time_ms(), packet_ssrc);
        }
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

    const bool send_success = SendPacketToNetwork(std::move(packet), std::move(options));

    // NOTE: The `packet` was moved to other, DO NOT use it any more.

    if (send_success) {
        // |media_has_been_sent_| is used by RTPSender to figure out if it can send
        // padding in the absence of transport-cc or abs-send-time.
        // In those cases media must be sent first to set a reference timestamp.
        media_has_been_sent_ = true;

        // TODO: Add support for FEC protecting all header extensions, 
        // add media packet to generator here instead.
        worker_queue_->Post([this, now_ms, send_stats=std::move(send_stats)](){
            UpdateSentStatistics(now_ms, std::move(send_stats));
        });
    }

    return send_success;
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

DataRate RtpPacketEgresser::GetSendBitrate(RtpPacketType packet_type) {
    RTC_RUN_ON(&sequence_checker_);
    return CalcSendBitrate(packet_type, clock_->now_ms());
}

// Private methods
DataRate RtpPacketEgresser::CalcSendBitrate(RtpPacketType packet_type, const int64_t now_ms) {
    auto it = send_bitrate_stats_.find(packet_type);
    if (it != send_bitrate_stats_.end()) {
        return it->second.Rate(now_ms).value_or(DataRate::Zero());
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
        auto packet_id = options.packet_id;
        int sent_size = send_transport_->SendRtpPacket(std::move(packet), std::move(options), false);
        // NOTE: The |sent_size| may be greater then size of the packet to send,
        // since it will be processed before sending to network, like encryption. 
        if (sent_size < 0) {
            PLOG_WARNING << "Faild to send packet: " << packet.sequence_number();
            return false;
        }
        if (transport_feedback_observer_) {
            RtpSentPacket sent_packet(clock_->CurrentTime(), packet_id);
            sent_packet.size = sent_size;
            transport_feedback_observer_->OnSentPacket(sent_packet);
        }
    }
    return false;
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

void RtpPacketEgresser::AddPacketToTransportFeedback(uint16_t packet_id, 
                                                     const RtpPacketToSend& packet,
                                                     std::optional<const PacedPacketInfo> pacing_info) {
    if (transport_feedback_observer_) {
        const size_t packet_size = send_side_bwe_with_overhead_ ? packet.size() 
                                                                : packet.payload_size() + packet.padding_size();
        
        RtpPacketSendInfo packet_info;
        packet_info.packet_id = packet_id;
        packet_info.rtp_timestamp = packet.timestamp();
        packet_info.packet_size = packet_size;
        packet_info.packet_type = packet.packet_type();
        packet_info.ssrc = packet.ssrc();
        packet_info.sequence_number = packet.sequence_number();
        packet_info.pacing_info = pacing_info;

        switch (packet.packet_type())
        {
        case RtpPacketType::AUDIO:
        case RtpPacketType::VIDEO:
            packet_info.media_ssrc = ssrc_;
            break;
        case RtpPacketType::RETRANSMISSION:
            // For retransmissions, we're want to remove the original media packet
            // if the rentrasmit arrives, so populate that in the packet send statistics.
            packet_info.media_ssrc = ssrc_;
            packet_info.sequence_number = *packet.retransmitted_sequence_number();
            break;
        case RtpPacketType::PADDING:
        case RtpPacketType::FEC:
            // We're not interested in feedback about these packets being received
            // or lost.
            break;
        default:
            break;
        }

        transport_feedback_observer_->OnAddPacket(packet_info);
    }
}

void RtpPacketEgresser::UpdateSentStatistics(const int64_t now_ms, SendStats send_stats) {
    // NOTE: We will send the retransmitted packets and padding packets by RTX stream, but:
    // 1) the RTX stream can send either the retransmitted packets or the padding packets
    // 2) the retransmitted packets can be sent by either the meida stream or the RTX stream
    // see https://blog.csdn.net/sonysuqin/article/details/82021185
    RtpStreamDataCounters* send_counters = send_stats.ssrc == rtx_ssrc_ ? &rtx_send_counter_ : &rtp_send_counter_;

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
    send_bitrate_stats_[send_stats.packet_type].Update(send_stats.packet_size, now_ms);

    if (send_bitrates_observer_) {
        if (send_bitrates_observer_) {
            const int64_t now_ms = clock_->now_ms();
            send_bitrates_observer_->OnSendBitratesUpdated(CalcTotalSendBitrate(now_ms).bps(),
                                                           CalcSendBitrate(RtpPacketType::RETRANSMISSION, now_ms).bps(), 
                                                           ssrc_);
        }
    }
}

void RtpPacketEgresser::UpdateDelayStatistics(int64_t new_delay_ms,
                                              int64_t now_ms,
                                              uint32_t ssrc) {
    if (!send_delay_observer_ || new_delay_ms < 0) {
        return;
    }

    // TODO: Create a helper class named RtpSendDelayStatistics.

    // Remove elements older than kSendSideDelayWindowMs
    auto lower_bound = send_delays_.lower_bound(now_ms - kSendSideDelayWindowMs);
    for (auto it = send_delays_.begin(); it != lower_bound; ++it) {
        if (max_delay_it_->second == it->second) {
            max_delay_it_ = send_delays_.end();
        }
        sliding_sum_delay_ms_ -= it->second;
    }
    send_delays_.erase(send_delays_.begin(), lower_bound);
    // The previous max was removed, we need to recompute.
    if (max_delay_it_ == send_delays_.end()) {
        RecalculateMaxDelay();
    }
    auto [it, inserted] = send_delays_.insert({now_ms, new_delay_ms});
    // Repalce the old one with the resent one.
    if (!inserted) {
        // Keep the most resent one if we have multiple delay meansurements
        // during the same time.
        int previous_delay_ms = it->second;
        sliding_sum_delay_ms_ -= previous_delay_ms;
        it->second = new_delay_ms;
        if (max_delay_it_ == it && new_delay_ms < previous_delay_ms) {
            RecalculateMaxDelay();
        }
    }
    if (max_delay_it_ == send_delays_.end() || 
        max_delay_it_->second < new_delay_ms) {
        max_delay_it_ = it;
    }
    sliding_sum_delay_ms_ += new_delay_ms;
    accumulated_delay_ms_ += new_delay_ms;
    size_t num_delays = send_delays_.size();
    int64_t avg_delay_ms = (sliding_sum_delay_ms_ + num_delays / 2) / num_delays;
    assert(avg_delay_ms >= 0);
    assert(max_delay_it_ != send_delays_.end());
    send_delay_observer_->OnSendDelayUpdated(avg_delay_ms, max_delay_it_->second, accumulated_delay_ms_, ssrc);
}

void RtpPacketEgresser::RecalculateMaxDelay() {
    max_delay_it_ = send_delays_.begin();
    for (auto it = send_delays_.begin(); it != send_delays_.end(); ++it) {
        if (it->second >= max_delay_it_->second) {
            max_delay_it_ = it;
        }
    }
}

void RtpPacketEgresser::PeriodicUpdate() {
    if (send_bitrates_observer_) {
        const int64_t now_ms = clock_->now_ms();
        send_bitrates_observer_->OnSendBitratesUpdated(CalcTotalSendBitrate(now_ms).bps(),
                                                       CalcSendBitrate(RtpPacketType::RETRANSMISSION, now_ms).bps(), 
                                                       ssrc_);
    }
}

std::optional<uint16_t> RtpPacketEgresser::PrepareForSend(RtpPacketToSend& packet) {
    // Assign sequence numbers, but not for flexfec which is already running on
    // an internally maintained sequence number series.
    if (!flex_fec_ssrc_ || packet.ssrc() != *flex_fec_ssrc_) {
        seq_num_assigner_->Sequence(packet);
    }
    // Assign transport sequence number.
    uint16_t packet_id = (++transport_sequence_number_) & 0xFFFF;
    if (packet.SetExtension<rtp::TransportSequenceNumber>(packet_id)) {
        return packet_id;
    } else {
        --transport_sequence_number_;
        return std::nullopt;
    }
}
    
} // namespace naivertc
