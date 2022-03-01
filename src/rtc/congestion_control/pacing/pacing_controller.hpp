#ifndef _RTC_CONGESTION_CONTROL_PACING_PACING_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROL_PACING_PACING_CONTROLLER_H_

#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"

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

    // Configuration
    struct Configuration {
        bool drain_large_queue = true;
        bool send_padding_if_silent = false;
        bool pace_audio = false;
        bool ignore_transport_overhead = false;
        TimeDelta padding_target_duration = TimeDelta::Millis(5);

        Clock* clock = nullptr;
        PacketSender* packet_sender = nullptr;
    };
public:
    PacingController(const Configuration& config);
    ~PacingController();

private:
    const bool drain_large_queue_;
    const bool send_padding_if_silent_;
    const bool pace_audio_;
    const bool ignore_transport_overhead_;
    const TimeDelta padding_target_duration_;

    const Clock* clock_;
    const PacketSender* packet_sender_;
};
    
} // namespace naivertc


#endif
