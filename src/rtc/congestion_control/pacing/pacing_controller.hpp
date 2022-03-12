#ifndef _RTC_CONGESTION_CONTROL_PACING_PACING_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROL_PACING_PACING_CONTROLLER_H_

#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/congestion_control/pacing/bitrate_prober.hpp"
#include "rtc/congestion_control/pacing/round_robin_packet_queue.hpp"

#include <optional>
#include <vector>

namespace naivertc {

class Clock;

class PacingController {
public:
    // PacketSender
    class PacketSender {
    public:
        virtual ~PacketSender() = default;
        virtual void SendPacket(RtpPacketToSend packet, 
                                const PacedPacketInfo& pacing_info) = 0;
        // Should be called after each call to SendPacket().
        virtual std::vector<RtpPacketToSend> FetchFecPackets() = 0;
        virtual std::vector<RtpPacketToSend> GeneratePadding(size_t padding_size) = 0;
    };

    struct PacingSettings {
        // "WebRTC-Pacer-DrainQueue/Enabled/"
        bool drain_large_queue = true;
        // "WebRTC-Pacer-PadInSilence/Disabled/"
        bool send_padding_if_silent = false;
        // "WebRTC-Pacer-BlockAudio/Disabled/"
        bool pacing_audio = false;
        // "WebRTC-Pacer-IgnoreTransportOverhead/Disabled"
        bool ignore_transport_overhead = false;
        // "WebRTC-Pacer-DynamicPaddingTarget/timedelta:10ms/"
        TimeDelta padding_target_duration = TimeDelta::Millis(5);
    };

    using ProbingSetting = BitrateProber::Configuration;
    // Configuration
    struct Configuration {
        PacingSettings pacing_setting;
        ProbingSetting probing_setting;

        Clock* clock = nullptr;
        PacketSender* packet_sender = nullptr;
    };

    // Expected max pacer delay. If ExpectedQueueTime() is higher than
    // this value, the packet producers should wait (eg drop frames rather than
    // encoding them). Bitrate sent may temporarily exceed target set by
    // UpdateBitrate() so that this limit will be upheld.
    static const TimeDelta kMaxExpectedQueueTime;
    // Pacing-rate relative to our target send rate.
    // Multiplicative factor that is applied to the target bitrate to calculate
    // the number of bytes that can be transmitted per interval.
    // Increasing this factor will result in lower delays in cases of bitrate
    // overshoots from the encoder.
    static const float kDefaultPaceMultiplier;
    // If no media or paused, wake up at least every |kPausedProcessInterval| in
    // order to send a keep-alive packet so we don't get stuck in a bad state due
    // to lack of feedback.
    static const TimeDelta kPausedProcessInterval;
    // Allow probes to be processed slightly ahead of inteded send time. Currently
    // set to 1 ms as this is intended to allow times be rounded down to the nearest
    // millisecond.
    static const TimeDelta kMaxEarlyProbeProcessing;
public:
    PacingController(const Configuration& config);
    ~PacingController();

    bool include_overhead() const;
    void set_include_overhead();

    size_t transport_overhead() const;
    void set_transport_overhead(size_t overhead_per_packet);

    bool account_for_audio() const;
    void set_account_for_audio(bool account_for_audio);

    std::optional<Timestamp> first_sent_packet_time() const;

    void Pause();
    void Resume();

    void SetProbingEnabled(bool enabled);

    void SetPacingBitrate(DataRate pacing_bitrate, 
                          DataRate padding_bitrate);

    void SetCongestionWindow(size_t congestion_window_size);
    void OnInflightBytes(size_t inflight_bytes);

    // Adds the packet to the queue and calls PacketSender::SendPacket() when
    // it's time to send.
    bool EnqueuePacket(RtpPacketToSend packet);

    bool AddProbeCluster(int cluster_id, 
                         DataRate target_bitrate);

    void ProcessPackets();

    Timestamp NextSendTime() const;

    bool IsCongested() const;

    size_t NumQueuedPackets() const;

    Timestamp OldestPacketEnqueueTime() const;

    TimeDelta ExpectedQueueTime() const;

private:
    void EnqueuePacketInternal(RtpPacketToSend packet, 
                               const int priority);

    std::pair<TimeDelta, bool> UpdateProcessTime(Timestamp at_time);

    void ReduceDebt(TimeDelta elapsed_time);

    void AddDebt(size_t sent_bytes);

    bool IsTimeToSendHeartbeat(Timestamp at_time) const;

    void OnMediaSent(RtpPacketType packet_type, 
                     size_t sent_bytes, 
                     Timestamp at_time);
    void OnPaddingSent(size_t sent_bytes, 
                       Timestamp at_time);

    std::optional<RtpPacketToSend> 
    NextPacketToSend(const PacedPacketInfo& pacing_info,
                     Timestamp target_send_time,
                     Timestamp at_time); 

    size_t PaddingSizeToAdd(size_t recommended_probe_size, size_t sent_bytes);

    inline TimeDelta TimeToPayOffMediaDebt() const;
    inline TimeDelta TimeToPayOffPaddingDebt() const;
                
private:
    const PacingSettings pacing_setting_;
    Clock* const clock_;
    PacketSender* const packet_sender_;

    size_t media_debt_ = 0;
    size_t padding_debt_ = 0;

    DataRate media_bitrate_ = DataRate::Zero();
    DataRate padding_bitrate_ = DataRate::Zero();
    DataRate pacing_bitrate_ = DataRate::Zero();

    Timestamp last_process_time_;
    Timestamp last_send_time_;
    std::optional<Timestamp> first_sent_packet_time_;

    bool probing_send_failure_ = false;
    BitrateProber prober_;

    bool paused_ = false;
    uint64_t packet_counter_ = 0;
    RoundRobinPacketQueue packet_queue_;

    size_t congestion_window_size_ = 0;
    size_t inflight_bytes_ = 0;
    // Account for audio: so that audio packets can cause pushback on other
    // types such as video. But audio packet should still be immediated passed
    // through though.
    bool account_for_audio_ = false;

    TimeDelta queue_time_cap_;
};
    
} // namespace naivertc


#endif
