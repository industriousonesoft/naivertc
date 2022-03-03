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
        bool pace_audio = false;
        bool ignore_transport_overhead = false;
        TimeDelta padding_target_duration = TimeDelta::Millis(5);

        ProbingSetting probing_setting;

        Clock* clock = nullptr;
        PacketSender* packet_sender = nullptr;
    };
public:
    PacingController(const Configuration& config);
    ~PacingController();

    void SetPacingBitrate(DataRate pacing_bitrate, 
                          DataRate padding_bitrate);

    void SetCongestionWindow(size_t window_size);

    // Adds the packet to the queue and calls PacketSender::SendPacket() when
    // it's time to send.
    void EnqueuePacket(RtpPacketToSend packet);

private:
    int PriorityForType(RtpPacketType packet_type);

    void EnqueuePacketInternal(RtpPacketToSend packet, 
                               const int priority);

private:
    const bool drain_large_queue_;
    const bool send_padding_if_silent_;
    const bool pace_audio_;
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

    BitrateProber prober_;
    RoundRobinPacketQueue packet_queue_;

};
    
} // namespace naivertc


#endif
