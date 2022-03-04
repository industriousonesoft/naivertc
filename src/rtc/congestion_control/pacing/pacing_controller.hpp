#ifndef _RTC_CONGESTION_CONTROL_PACING_PACING_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROL_PACING_PACING_CONTROLLER_H_

#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_control/components/interval_budget.hpp"
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
        virtual void SendPacket(RtpPacketToSend packet) = 0;
        // Should be called after each call to SendPacket().
        virtual std::vector<RtpPacketToSend> FetchFecPackets() = 0;
        virtual std::vector<RtpPacketToSend> GeneratePadding(size_t bytes) = 0;
    };

    using ProbingSetting = BitrateProber::Configuration;
    // Configuration
    struct Configuration {
        bool include_overhead = false;
        bool drain_large_queue = true;
        bool send_padding_if_silent = false;
        bool pacing_audio = false;
        bool ignore_transport_overhead = false;
        TimeDelta padding_target_duration = TimeDelta::Millis(5);

        ProbingSetting probing_setting;

        Clock* clock = nullptr;
        PacketSender* packet_sender = nullptr;
    };

    // Expected max pacer delay. If ExpectedQueueTime() is higher than
    // this value, the packet producers should wait (eg drop frames rather than
    // encoding them). Bitrate sent may temporarily exceed target set by
    // UpdateBitrate() so that this limit will be upheld.
    static const TimeDelta kMaxExpectedQueueLength;
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
public:
    PacingController(const Configuration& config);
    ~PacingController();

    void SetPacingBitrate(DataRate pacing_bitrate, 
                          DataRate padding_bitrate);

    void SetCongestionWindow(size_t congestion_window_size);
    void OnInflightBytes(size_t inflight_bytes);

    // Adds the packet to the queue and calls PacketSender::SendPacket() when
    // it's time to send.
    void EnqueuePacket(RtpPacketToSend packet);

    Timestamp NextSendTime() const;

    bool IsCongested() const;

private:
    void EnqueuePacketInternal(RtpPacketToSend packet, 
                               const int priority);

private:
    const bool drain_large_queue_;
    const bool send_padding_if_silent_;
    const bool pacing_audio_;
    const bool ignore_transport_overhead_;
    const TimeDelta padding_target_duration_;

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

    IntervalBudget media_budget_;
    IntervalBudget padding_budget_;

    bool probing_send_failure_ = false;
    BitrateProber prober_;

    bool paused_ = false;
    uint64_t packet_counter_ = 0;
    RoundRobinPacketQueue packet_queue_;

    size_t congestion_window_size_ = 0;
    size_t inflight_bytes_ = 0;

};
    
} // namespace naivertc


#endif
