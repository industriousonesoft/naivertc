#ifndef _RTC_RTP_RTCP_RTP_RTCP_INTERFACE_H_
#define _RTC_RTP_RTCP_RTP_RTCP_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

#include <optional>
#include <vector>
#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT RtpRtcpInterface {
public:
    struct Configuration {
        // True for a audio version of the RTP/RTCP module object false will create
        // a video version.
        bool audio = false;
    
        size_t rtcp_report_interval_ms = 0;
        
        // Corresponds to extmap-allow-mixed in SDP negotiation.
        bool extmap_allow_mixed = false;

        // SSRCs for media and retransmission(RTX), respectively.
        // FlexFec SSRC is fetched from |flexfec_sender|.
        uint32_t local_media_ssrc = 0;
        std::optional<uint32_t> rtx_send_ssrc = std::nullopt;

        // If true, the RTP packet history will select RTX packets based on
        // heuristics such as send time, retransmission count etc, in order to
        // make padding potentially more useful.
        // If false, the last packet will always be picked. This may reduce CPU
        // overhead.
        bool enable_rtx_padding_prioritization = true;

        // The clock to use to read time. If nullptr then system clock will be used.
        std::shared_ptr<Clock> clock = nullptr;

        std::shared_ptr<Transport> send_transport;

        std::shared_ptr<FecGenerator> fec_generator;

    };

public:
    // ======== Receiver methods ========
    virtual void IncomingRtcpPacket(const uint8_t* incoming_packet, size_t incoming_packet_size) = 0;
    virtual void SetRemoteSsrc(uint32_t ssrc) = 0;
    virtual void SetLocalSsrc(uint32_t ssrc) = 0;

    // ======== Sender methods ========
    // Sets the maximum size of an RTP packet, including RTP headers.
    virtual void SetMaxRtpPacketSize(size_t size) = 0;

    // Returns max RTP packet size. Takes into account RTP headers and
    // FEC/ULP/RED overhead (when FEC is enabled).
    virtual size_t MaxRtpPacketSize() const = 0;

    virtual void RegisterSendPayloadFrequency(int payload_type, int payload_frequency) = 0;
    virtual int32_t DeRegisterSendPayload(int8_t payload_type) = 0;

    // Returns current sending status.
    virtual bool Sending() const = 0;

    // Starts/Stops media packets. On by default.
    virtual void SetSendingMediaStatus(bool sending) = 0;

    // Returns current media sending status.
    virtual bool SendingMedia() const = 0;

    // Record that a frame is about to be sent. Returns true on success, and false
    // if the module isn't ready to send.
    virtual bool OnSendingRtpFrame(uint32_t timestamp,
                                   int64_t capture_time_ms,
                                   int payload_type,
                                   bool force_sender_report) = 0;

    // Try to send the provided packet. Returns true if packet matches any of
    // the SSRCs for this module (media/rtx/fec etc) and was forwarded to the
    // transport.
    virtual bool TrySendPacket(RtpPacketToSend* packet) = 0;

    virtual void OnPacketsAcknowledged(std::vector<uint16_t> sequence_numbers) = 0;

    // ======== RTCP ========
    // Returns remote NTP.
    // Returns -1 on failure else 0.
    virtual int32_t RemoteNTP(uint32_t* received_ntp_secs,
                                uint32_t* received_ntp_frac,
                                uint32_t* rtcp_arrival_time_secs,
                                uint32_t* rtcp_arrival_time_frac,
                                uint32_t* rtcp_timestamp) const = 0;

    // Returns current RTT (round-trip time) estimate.
    // Returns -1 on failure else 0.
    virtual int32_t RTT(uint32_t remote_ssrc,
                        int64_t* rtt,
                        int64_t* avg_rtt,
                        int64_t* min_rtt,
                        int64_t* max_rtt) const = 0;

    // Returns the estimated RTT, with fallback to a default value.
    virtual int64_t ExpectedRetransmissionTimeMs() const = 0;

    // Forces a send of a RTCP packet. Periodic SR and RR are triggered via the
    // process function.
    // Returns -1 on failure else 0.
    virtual int32_t SendRTCP(RtcpPacketType rtcp_packet_type) = 0;
    
    // ======== NACK ========
    // Store the sent packets, needed to answer to a Negative acknowledgment
    // requests.
    virtual void SetStorePacketsStatus(bool enable, uint16_t numberToStore) = 0;

};
    
} // namespace naivertc


#endif