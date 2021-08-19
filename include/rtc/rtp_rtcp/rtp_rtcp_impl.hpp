#ifndef _RTC_RTP_RTCP_RTP_RTCP_IMPL_H_
#define _RTC_RTP_RTCP_RTP_RTCP_IMPL_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interface.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpRtcpImpl : public RtpRtcpInterface,
                                   public RtcpReceiver::Observer {
public:
    RtpRtcpImpl(const RtpRtcpInterface::Configuration& config, std::shared_ptr<TaskQueue> task_queue);
    ~RtpRtcpImpl();

    // ======== Receiver methods ========
    void IncomingRtcpPacket(const uint8_t* incoming_packet, size_t incoming_packet_size) override;
    void SetRemoteSsrc(uint32_t ssrc) override;
    void SetLocalSsrc(uint32_t ssrc) override;

    // ======== Sender methods ========
    // Sets the maximum size of an RTP packet, including RTP headers.
    void SetMaxRtpPacketSize(size_t size) override;

    // Returns max RTP packet size. Takes into account RTP headers and
    // FEC/ULP/RED overhead (when FEC is enabled).
    size_t MaxRtpPacketSize() const override;

    void RegisterSendPayloadFrequency(int payload_type, int payload_frequency) override;
    int32_t DeRegisterSendPayload(int8_t payload_type) override;

    // Returns current sending status.
    bool Sending() const override;

    // Starts/Stops media packets. On by default.
    void SetSendingMediaStatus(bool sending) override;

    // Returns current media sending status.
    bool SendingMedia() const override;

    // Record that a frame is about to be sent. Returns true on success, and false
    // if the module isn't ready to send.
    bool OnSendingRtpFrame(uint32_t timestamp,
                                   int64_t capture_time_ms,
                                   int payload_type,
                                   bool force_sender_report) override;

    // Try to send the provided packet. Returns true if packet matches any of
    // the SSRCs for this module (media/rtx/fec etc) and was forwarded to the
    // transport.
    bool TrySendPacket(RtpPacketToSend* packet) override;

    void OnPacketsAcknowledged(std::vector<uint16_t> sequence_numbers) override;

    RtcpSender::FeedbackState GetFeedbackState();

    // ======== RTCP ========
    // Returns remote NTP.
    // Returns -1 on failure else 0.
    int32_t RemoteNTP(uint32_t* received_ntp_secs,
                                uint32_t* received_ntp_frac,
                                uint32_t* rtcp_arrival_time_secs,
                                uint32_t* rtcp_arrival_time_frac,
                                uint32_t* rtcp_timestamp) const override;

    // Returns current RTT (round-trip time) estimate.
    // Returns -1 on failure else 0.
    int32_t RTT(uint32_t remote_ssrc,
                        int64_t* rtt,
                        int64_t* avg_rtt,
                        int64_t* min_rtt,
                        int64_t* max_rtt) const override;

    // Returns the estimated RTT, with fallback to a default value.
    int64_t ExpectedRetransmissionTimeMs() const override;

    // Forces a send of a RTCP packet. Periodic SR and RR are triggered via the
    // process function.
    // Returns -1 on failure else 0.
    int32_t SendRTCP(RtcpPacketType rtcp_packet_type) override;
    
    // NACK
    // Store the sent packets, needed to answer to a Negative acknowledgment
    // requests.
    void SetStorePacketsStatus(bool enable, uint16_t numberToStore) override;

private:
    void MaybeSendRtcp();
    // Called when |rtcp_sender_| informs of the next RTCP instant. The method may
    // be called on various sequences, and is called under a RTCPSenderLock.
    void ScheduleRtcpSendEvaluation(TimeDelta duration);

    // Helper method combating too early delayed calls from task queues.
    void MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time);

    // Schedules a call to MaybeSendRtcpAtOrAfterTimestamp delayed by |duration|.
    void ScheduleMaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time, TimeDelta duration);

private:
    // Rtcp Receiver Observer
    void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) override;
    void OnRequestSendReport() override;
    void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) override;
    void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) override;  

private: 
    std::shared_ptr<TaskQueue> task_queue_;
    std::shared_ptr<Clock> clock_;
    RtcpSender rtcp_sender_;
    RtcpReceiver rtcp_receiver_;

};

} // namespace naivertc

#endif