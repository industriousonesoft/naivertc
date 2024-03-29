#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/time/clock.hpp"

#include <plog/Log.h>

#include "testing/defines.hpp"

namespace naivertc {
namespace {

constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis(500);

// The maximum debt level, in terms of time, capped when sending packets.
constexpr TimeDelta kMaxDebtInTime = TimeDelta::Millis(500); // 500ms
constexpr TimeDelta kMaxElapsedTime = TimeDelta::Seconds(2); // 2s

constexpr int kDefaultPriority = 0;

int PriorityForType(RtpPacketType packet_type) {
  // Lower numbers takes priority over higher number.
    switch (packet_type)
    {
    case RtpPacketType::AUDIO:
        // Audio packet is always prioritized over others.
        return kDefaultPriority + 1;
    case RtpPacketType::RETRANSMISSION:
        // Send retransmission packet before new media.
        return kDefaultPriority + 2;
    case RtpPacketType::VIDEO:
    case RtpPacketType::FEC:
        // Video packet has normal priority.
        // Send FEC packet concurrently to video packet, as
        // the FEC packet might have a lower chance of being
        // useful if delayed.
        return kDefaultPriority + 3;
    case RtpPacketType::PADDING:
        // The padding is likey useless, only sent to keep 
        // the bandwidth high.
        return kDefaultPriority;
    }
}
  
} // namespace

const TimeDelta PacingController::kMaxExpectedQueueTime = TimeDelta::Millis(2000); // 2s
const float PacingController::kDefaultPaceMultiplier = 2.5f;
const TimeDelta PacingController::kPausedProcessInterval = kCongestedPacketInterval; // 500 ms
const TimeDelta PacingController::kMaxEarlyProbeProcessing = TimeDelta::Millis(1); // 1 ms

PacingController::PacingController(const Configuration& config) 
    : pacing_settings_(config.pacing_settings),
      clock_(config.clock),
      packet_sender_(config.packet_sender),
      last_process_time_(clock_->CurrentTime()),
      last_send_time_(last_process_time_),
      prober_(config.probing_settings),
      packet_queue_(last_process_time_),
      queue_time_cap_(kMaxExpectedQueueTime) {
    // assert(packet_sender_ != nullptr);
}

PacingController::~PacingController() {}

bool PacingController::include_overhead() const {
    return packet_queue_.include_overhead();
}

void PacingController::set_include_overhead() {
    packet_queue_.set_include_overhead();
}

size_t PacingController::transport_overhead() const {
    return packet_queue_.transport_overhead();
}

void PacingController::set_transport_overhead(size_t overhead_per_packet) {
    if (pacing_settings_.ignore_transport_overhead) {
        return;
    }
    packet_queue_.set_transport_overhead(overhead_per_packet);
}

bool PacingController::account_for_audio() const {
    return account_for_audio_;
}   

void PacingController::set_account_for_audio(bool account_for_audio) {
    account_for_audio_ = account_for_audio;
}

TimeDelta PacingController::queue_time_cap() const {
    return queue_time_cap_;
}
    
void PacingController::set_queue_time_cap(TimeDelta cap) {
    queue_time_cap_ = cap;
}

std::optional<Timestamp> PacingController::first_sent_packet_time() const {
    return first_sent_packet_time_;
}

DataRate PacingController::pacing_bitrate() const {
    return pacing_bitrate_;
}

void PacingController::Pause() {
    if (!paused_) {
        PLOG_INFO << "PacedSender paused.";
    }
    paused_ = true;
    packet_queue_.SetPauseState(paused_, clock_->CurrentTime());
}

void PacingController::Resume() {
    if (paused_) {
        PLOG_INFO << "PacedSender resumed.";
    }
    paused_ = false;
    packet_queue_.SetPauseState(paused_, clock_->CurrentTime());
}

void PacingController::SetProbingEnabled(bool enabled) {
    prober_.SetEnabled(enabled);
}

void PacingController::SetPacingBitrates(DataRate pacing_bitrate, 
                                         DataRate padding_bitrate) {
    media_bitrate_ = pacing_bitrate;
    padding_bitrate_ = padding_bitrate;
    pacing_bitrate_ = pacing_bitrate;

    PLOG_VERBOSE << "Set pacing bitrate=" << pacing_bitrate.bps() 
                 << " bps, padding bitrate=" << padding_bitrate.bps()
                 << " bps.";
}

void PacingController::SetCongestionWindow(size_t congestion_window_size) {
    const bool was_congested = IsCongested();
    congestion_window_size_ = congestion_window_size;
    if (was_congested && !IsCongested()) {
        // Update last process time when the congestion state changed.
        // FIXME: But why do we need to do this?
        auto [elapsed_time, update] = UpdateProcessTime(clock_->CurrentTime());
        if (update && elapsed_time > TimeDelta::Zero()) {
            ReduceDebt(elapsed_time);
        }
    }
}

void PacingController::OnInflightBytes(size_t inflight_bytes) {
    const bool was_congested = IsCongested();
    inflight_bytes_ = inflight_bytes;
    if (was_congested && !IsCongested()) {
        // Update last process when the congestion state changed.
        auto [elapsed_time, update] = UpdateProcessTime(clock_->CurrentTime());
        if (update && elapsed_time > TimeDelta::Zero()) {
            ReduceDebt(elapsed_time);
        }
    }
}

bool PacingController::EnqueuePacket(RtpPacketToSend packet) {
    if (pacing_bitrate_ <= DataRate::Zero()) {
        PLOG_WARNING << "The pacing bitrate must be set before enqueuing packet.";
        return false;
    }
    EnqueuePacketInternal(std::move(packet), PriorityForType(packet.packet_type()));
    return true;
}

bool PacingController::AddProbeCluster(int cluster_id, 
                                       DataRate target_bitrate) {
    return prober_.AddProbeCluster(cluster_id, target_bitrate, clock_->CurrentTime());
}

void PacingController::ProcessPackets() {
    auto now = clock_->CurrentTime();
    auto target_send_time = NextSendTime();
    // NOTE: Probing should be processed earlier.
    TimeDelta early_execute_margin = prober_.IsProbing() ? kMaxEarlyProbeProcessing : TimeDelta::Zero();

    if (target_send_time.IsMinusInfinity()) {
        target_send_time = now;
    } else if (now < target_send_time - early_execute_margin) {
        // We are too early, but if queue is empty still allow draining some debt.
        auto [elapsed_time, updated] = UpdateProcessTime(now);
        if (updated && elapsed_time > TimeDelta::Zero()) {
            ReduceDebt(elapsed_time);
        }
        return;
    }

    if (target_send_time < last_process_time_) {
        PLOG_WARNING << "The next sent time is older than the last process time.";
        // FIXME: When dose this situation happen?
        ReduceDebt(last_process_time_ - target_send_time);
        target_send_time = last_process_time_;
    }

    auto prev_process_time = last_process_time_;
    TimeDelta elapsed_time = UpdateProcessTime(now).first;

    // Check if it's time to send a heartbeat packet.
    if (IsTimeToSendHeartbeat(now)) {
        if (packet_counter_ == 0) {
            // We can not send padding until a media packet has first been sent.
            last_send_time_ = now;
        } else {
            // Generate and send padding packets.
            size_t sent_bytes = 0;
            auto heartbeat_packets = packet_sender_->GeneratePadding(/*size=*/1);
            for (auto& packet : heartbeat_packets) {
                sent_bytes += packet.payload_size() + packet.padding_size();
                packet_sender_->SendPacket(std::move(packet), PacedPacketInfo());
                // FEC protected.
                for (auto& fec_packet : packet_sender_->FetchFecPackets()) {
                    EnqueuePacket(std::move(fec_packet));
                }
            }
            OnPaddingSent(sent_bytes, now);
        }
    }

    if (paused_) {
        return;
    }

    if (elapsed_time > TimeDelta::Zero()) {
        auto target_bitrate = pacing_bitrate_;
        size_t queued_packet_size = packet_queue_.queued_size();
        if (queued_packet_size > 0) {
            packet_queue_.UpdateEnqueueTime(now);
            if (pacing_settings_.drain_large_queue) {
                auto avg_time_left = std::max(TimeDelta::Millis(1), queue_time_cap_ - packet_queue_.AverageQueueTime());
                // The minimum bitrate required to drain queue.
                auto min_drain_bitrate_required = queued_packet_size / avg_time_left;
                if (min_drain_bitrate_required > target_bitrate) {
                    target_bitrate = min_drain_bitrate_required;
                    PLOG_WARNING << "Update target bitrate (" << target_bitrate.bps()
                                 << " bps) to drain bitrate (" << min_drain_bitrate_required.bps() << " bps).";
                }
            }
        }
        media_bitrate_ = target_bitrate;
    }

    bool first_packet_in_probe = false;
    PacedPacketInfo pacing_info;
    size_t recommended_probe_size = 0;
    bool is_probing = prober_.IsProbing();
    if (is_probing) {
        // Probe timing is sensitive, and handled explicitly by BitrateProber,
        // so use actual sent time rather |target_send_time|.
        pacing_info.probe_cluster = prober_.CurrentProbeCluster(now);
        if (pacing_info.probe_cluster.has_value()) {
            first_packet_in_probe = pacing_info.probe_cluster->sent_bytes == 0;
            recommended_probe_size = prober_.RecommendedMinProbeSize();
        } else {
            // No valid probe cluster returned, probe might have timed out.
            is_probing = false;
        }
    }

    size_t sent_bytes = 0;

    // NOTE: 进入process循环后会根据包的优先级进行处理，即先发送优先级高的包：
    // probe > audio > paced packets (retransmission > video|FEC) > padding.
    // 如果当前优先级的包已经发送完，则检查并发送下一个优先级的包，以此类推。直到
    // 在此次需要发送的包都已经全部发送，或因probe才会退出循环。
    while (!paused_) {
        if (first_packet_in_probe) {
            // FIXME: Why does probing starts with a small padding packet?
            // If it's first packet in probe, we insert a small padding packet so
            // we have a more reliable start window for the bitrate estimation.
            auto padding_packets = packet_sender_->GeneratePadding(1);
            if (!padding_packets.empty()) {
                // Should return only one padding packet with a requested size of 1 byte.
                assert(padding_packets.size() == 1);
                // Insert padding packet with high prioriy to make sure it won't be
                // preempt by media packets.
                EnqueuePacketInternal(std::move(padding_packets[0]), kDefaultPriority);
            }
            first_packet_in_probe = false;
        }

        // NOTE: 每次进入process的第一个循环时以下条件恒成立，期望是每次调用
        // process都能发送新包。
        if (prev_process_time < target_send_time) {
            // Reduce buffer levels with amount corresponding to time between last
            // process and target send time for the next packet.
            // If the process call is late, that may be the time between the optimal
            // send times for two packets we should already have sent.
            ReduceDebt(target_send_time - prev_process_time);
            prev_process_time = target_send_time;
        }

        std::optional<RtpPacketToSend> rtp_packet = NextPacketToSend(pacing_info, target_send_time, now);
        // No packet available to send.
        if (rtp_packet == std::nullopt) {
            // Check if we should send padding.
            size_t padding_to_add = PaddingSizeToAdd(recommended_probe_size, sent_bytes);
            if (padding_to_add > 0) {
                // GTEST_COUT << "padding_to_add=" << padding_to_add 
                //            << " - recommended_probe_size=" << recommended_probe_size
                //            << " - sent_bytes=" << sent_bytes
                //            << " - last_process_time=" << last_process_time_.ms()
                //            << " - probe_cluster_id=" << (pacing_info.probe_cluster.has_value() ? pacing_info.probe_cluster->id : -1)
                //            << std::endl;
                auto padding_packets = packet_sender_->GeneratePadding(padding_to_add);
                // Enqueue the padding packets.
                if (!padding_packets.empty()) {
                    for (auto& packet : padding_packets) {
                        EnqueuePacket(packet);
                    }
                    // Continue loop to send the padding that was just added.
                    continue;
                }
            }
            // Can't fetch new packet and no padding to send, exit send loop.
            break;
        }

        const RtpPacketType packet_type = rtp_packet->packet_type();
        size_t packet_size = rtp_packet->payload_size() + rtp_packet->padding_size();
        if (include_overhead()) {
            packet_size += rtp_packet->header_size() + transport_overhead();
        }

        // Send packet
        packet_sender_->SendPacket(std::move(*rtp_packet), pacing_info);
        // FIXME: Why does the padding need FEC protection too?
        // Enqueue FEC packet after sending.
        for (auto& fec_packet : packet_sender_->FetchFecPackets()) {
            EnqueuePacket(std::move(fec_packet));
        }
        sent_bytes += packet_size;

        OnMediaSent(packet_type, packet_size, target_send_time);

        // NOTE: Probing works by sending short bursts of RTP packet at a bitrate
        // that we wish to see, rather than sending packet continuously.
        // If we are currently probing, we need to stop the send loop 
        // when we have reached the send target.
        if (is_probing && sent_bytes >= recommended_probe_size) {
            break;
        }

        // NOTE: 如果下一次发包时间是在未来，即target_send_time = now，
        // 说明此次需要发送的media包都已经发送，接下来就检查是否需要发送padding包。
        // Update target send time in case that are more packets 
        // that we are late in processing.
        Timestamp next_sent_time = NextSendTime();
        if (next_sent_time.IsMinusInfinity()) {
            target_send_time = now;
        } else {
            target_send_time = std::min(now, next_sent_time);
        }

    } // end while

    last_process_time_ = std::max(last_process_time_, prev_process_time);

    if (is_probing) {
        probing_send_failure_ = sent_bytes == 0 ? true : false;
        if (!probing_send_failure_) {
            prober_.OnProbeSent(sent_bytes, clock_->CurrentTime());
        }
    }

}

Timestamp PacingController::NextSendTime() const {
    const Timestamp now = clock_->CurrentTime();

    // If paused, we only send heartbeats at intervals.
    if (paused_) {
        return last_send_time_ + kPausedProcessInterval;
    }

    // If probing is active, that always takes priority.
    if (prober_.IsProbing()) {
        Timestamp probe_time = prober_.NextTimeToProbe(now);
        if (probe_time != Timestamp::PlusInfinity() && !probing_send_failure_) {
            return probe_time;
        }
    }

    // If not pacing audio, audio packet takes a higher priority.
    if (!pacing_settings_.pacing_audio) {
        // return the enqueue time if the current leading packet is audio.
        auto audio_enqueue_time = packet_queue_.LeadingAudioPacketEnqueueTime();
        if (audio_enqueue_time) {
            return *audio_enqueue_time;
        }
    }

    // In congestion or haven't received any packet so far.
    if (IsCongested() || packet_counter_ == 0) {
        // We need to at least send keep-alive packets with some interval.
        return last_send_time_ + kCongestedPacketInterval;
    }

    // Send media packets first if we can.
    if (media_bitrate_ > DataRate::Zero() && !packet_queue_.Empty()) {
        // The next time we can send next media packet as soon as possible.
        // GTEST_COUT << "last_send_time=" << last_send_time_.ms()
        //            << " ms - last_process_time=" << last_process_time_.ms()
        //            << " ms - media_debt=" << media_debt_
        //            << " - paid_off_time=" << TimeToPayOffMediaDebt().ms()
        //            << " ms" << std::endl;
        return std::min(last_send_time_ + kPausedProcessInterval, 
                        last_process_time_ + TimeToPayOffMediaDebt());
    }

    // Send padding packet when no packets in queue.
    if (padding_bitrate_ > DataRate::Zero() && packet_queue_.Empty()) {
        // Both media and padding debts should be drained.
        TimeDelta drain_time = std::max(TimeToPayOffMediaDebt(), TimeToPayOffPaddingDebt());
        return std::min(last_send_time_ + kPausedProcessInterval,
                        last_process_time_ + drain_time);
    }

    // Send padding as heartbeat if necessary.
    if (pacing_settings_.send_padding_if_silent) {
        return last_send_time_ + kPausedProcessInterval;
    }

    return last_process_time_ + kPausedProcessInterval;
}

bool PacingController::IsCongested() const {
    if (congestion_window_size_ > 0) {
        return inflight_bytes_ >= congestion_window_size_;
    } 
    return false;
}

bool PacingController::IsProbing() const {
    return prober_.IsProbing();
}

size_t PacingController::NumQueuedPackets() const {
    return packet_queue_.num_packets();
}

size_t PacingController::QueuedPacketSize() const {
    return packet_queue_.queued_size();
}

Timestamp PacingController::OldestPacketEnqueueTime() const {
    return packet_queue_.OldestEnqueueTime();
}

TimeDelta PacingController::ExpectedQueueTime() const {
    return packet_queue_.queued_size() / pacing_bitrate_;
}

// Private methods
void PacingController::EnqueuePacketInternal(RtpPacketToSend packet, 
                                             const int priority) {
    prober_.OnIncomingPacket(packet.size());

    auto now = clock_->CurrentTime();
    
    if (packet_queue_.Empty()) {
        // If queue is empty, we need to "fast-forward" the last process time,
        // so that we don't use passed time as budget for sending the first new
        // packet.
        Timestamp target_process_time = now;
        Timestamp next_send_time = NextSendTime();
        if (next_send_time.IsFinite()) {
            // There was already a valid planned send time, such as a heartbeat.
            // Use that as last process time only if it's prior to now.
            target_process_time = std::min(now, next_send_time);
        }
        auto [elapsed_time, updated] = UpdateProcessTime(target_process_time);
        if (updated && elapsed_time > TimeDelta::Zero()) {
            ReduceDebt(elapsed_time);
        } else {
            last_process_time_ = target_process_time;
        }
    }
    packet_queue_.Push(priority, clock_->CurrentTime(), packet_counter_++, std::move(packet));
}

std::pair<TimeDelta, bool> PacingController::UpdateProcessTime(Timestamp at_time) {
    // If no previous process or the last process is in the future (as early probe process),
    // then there is no elapsed time to reduce debt for.
    if (last_process_time_.IsMinusInfinity() || at_time < last_process_time_) {
        return {TimeDelta::Zero(), false};
    }

    TimeDelta elapsed_time = at_time - last_process_time_;
    last_process_time_ = at_time;
    if (elapsed_time > kMaxElapsedTime) {
        PLOG_WARNING << "Elapsed time (" << elapsed_time.ms() 
                     << " ms) is longer than expected, limiting to "
                     << kMaxElapsedTime.ms() << " ms.";
        elapsed_time = kMaxElapsedTime;
    }
    return {elapsed_time, true};
}

void PacingController::ReduceDebt(TimeDelta elapsed_time) {
    // Make sure |media_debt_| and |padding_debt_| not becomes a negative.
    media_debt_ -= std::min(media_debt_, media_bitrate_ * elapsed_time);
    padding_debt_ -= std::min(padding_debt_, padding_bitrate_ * elapsed_time);
    // GTEST_COUT << "ReduceDebt media_debt=" << media_debt_ 
    //            << " - padding_debt_=" << padding_debt_ 
    //            << std::endl;
}

void PacingController::AddDebt(size_t sent_bytes) {
    inflight_bytes_ += sent_bytes;
    media_debt_ += sent_bytes;
    padding_debt_ += sent_bytes;
    media_debt_ = std::min(media_debt_, media_bitrate_ * kMaxDebtInTime);
    padding_debt_ = std::min(padding_debt_, padding_bitrate_ * kMaxDebtInTime);
    // GTEST_COUT << "AddDebt sent_bytes=" << sent_bytes 
    //            << " - media_debt=" << media_debt_
    //            << " - padding_debt=" << padding_debt_
    //            << std::endl;
}

bool PacingController::IsTimeToSendHeartbeat(Timestamp at_time) const {
    if (pacing_settings_.send_padding_if_silent || 
        paused_ || 
        IsCongested() || 
        packet_counter_ == 0) {
        // We send a padding packet as heartbeat every 500 ms to ensure we won't
        // get stuck in congested state due to no feedback being received.
        auto elapsed_since_last_send = at_time - last_send_time_;
        if (elapsed_since_last_send >= kCongestedPacketInterval) {
            return true;
        }
    }
    return false;
}

void PacingController::OnMediaSent(RtpPacketType packet_type, 
                                   size_t sent_bytes, 
                                   Timestamp at_time) {
    if (!first_sent_packet_time_) {
        first_sent_packet_time_ = at_time;
    }

    // Only account for audio packet as required.
    if (packet_type != RtpPacketType::AUDIO || account_for_audio_) {
        AddDebt(sent_bytes);
    }
    last_send_time_ = at_time;
    last_process_time_ = at_time;
}

void PacingController::OnPaddingSent(size_t sent_bytes, 
                                     Timestamp at_time) {
    if (sent_bytes > 0) {
        AddDebt(sent_bytes);
    }
    last_send_time_ = at_time;
    last_process_time_ = at_time;
}

std::optional<RtpPacketToSend> PacingController::NextPacketToSend(const PacedPacketInfo& pacing_info,
                                                                  Timestamp target_send_time,
                                                                  Timestamp at_time) {
    if (packet_queue_.Empty()) {
        return std::nullopt;
    }

    // NOTE: 音频包由于音频对连续性要求高，且因本身体积小不易丢包，因此可以选择忽略网络拥塞情况。
    // Check if the next packet to send is a unpaced audio packet.
    bool has_unpaced_audio_packet = !pacing_settings_.pacing_audio && packet_queue_.LeadingAudioPacketEnqueueTime().has_value();
    bool is_probing = pacing_info.probe_cluster.has_value();
    // If the next packet is not neither a audio nor used to probe,
    // we need to check it futher.
    if (!has_unpaced_audio_packet && !is_probing) {
        // NOTE: 如果目前仍处于拥塞情况则不再发送新包，因为新包只会进一步加剧拥塞。
        if (IsCongested()) {
            // Don't send any packets (except unpaced aduio packet or probe packet) if congested.
            return std::nullopt;
        } 
        // Allow sending slight early if we could.
        else if (at_time <= target_send_time) {
            // Check if we can paid off the debt (reduce debt to zero) at least 
            // at the target sent time.
            // NOTE: |time_to_paid_off|是一个估计值，预估之前发送的包到达接收端的理论时长。
            // 因为此时|media_debt_|对应的包可能已经抵达接受端，只是发送端还没收到反馈而已，
            // 故media_debt_可能未被及时更新。
            auto time_to_paid_off = TimeToPayOffMediaDebt();
            // GTEST_COUT << "at_time=" << at_time.ms() 
            //            << " ms - target_send_time=" << target_send_time.ms()
            //            << " ms - time_to_paid_off=" << time_to_paid_off.ms() << " ms."
            //            << std::endl;
            // NOTE: 如果此时|media_debt_|对应的包理论上还未抵达接收端，则暂时不发送新包。
            // 因为这只会进一步加剧网络拥塞。
            // 当|at_time + time_to_paid_off > target_send_time|表示即便是按时（at target_send_time）
            // 发送下一个包，已发送的旧包仍未抵达接收端，故暂时不发新包，以避免加剧拥塞。
            // 反之，如果在按时发送下一个包之前，旧包就已经抵达接收端，故可以提前发送新包，以减少延时。
            if (at_time + time_to_paid_off > target_send_time) {
                // Wait for next sent time.
                return std::nullopt;
            }
        }
    }
    // The next packet could be audio, probe or others.
    return packet_queue_.Pop();
}

size_t PacingController::PaddingSizeToAdd(size_t recommended_probe_size, size_t sent_bytes) {
    if (!packet_queue_.Empty()) {
        // No need to add padding if we have media packets in queue.
        return 0;
    }
    
    if (IsCongested()) {
        // Don't add padding if congested, even if requested for probing.
        return 0;
    }

    if (packet_counter_ == 0) {
        // Don't add padding until a media packet has first been sent.
        return 0;
    }

    if (recommended_probe_size > 0) {
        // Check if we need to send padding packet for probing.
        if (recommended_probe_size > sent_bytes) {
            // The remaining size for probing.
            return recommended_probe_size - sent_bytes;
        } else {
            return 0;
        }
    }

    // Only add new padding till all padding debt has paid off.
    if (padding_bitrate_ > DataRate::Zero() && padding_debt_ == 0) {
        return pacing_settings_.padding_target_duration * padding_bitrate_;
    }

    return 0;
}

inline TimeDelta PacingController::TimeToPayOffMediaDebt() const {
    return media_debt_ / media_bitrate_;
}

inline TimeDelta PacingController::TimeToPayOffPaddingDebt() const {
    return padding_debt_ / padding_bitrate_;
}
    
} // namespace naivertc

