#include "rtc/rtp_rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/rtp_feedback.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/nack.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/psfb.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/fir.hpp"
#include "common/utils_time.hpp"
#include "rtc/rtp_rtcp/time_util.hpp"
#include <plog/Log.h>

namespace naivertc {

constexpr int64_t kMaxWarningLogIntervalMs = 10000;

bool RtcpReceiver::ParseCompoundPacket(BinaryBuffer packet) {

    rtcp::CommonHeader rtcp_block;
    for (const uint8_t* next_block = packet.begin().base(); next_block != packet.end().base(); 
        next_block = rtcp_block.NextPacket() ) {
        ptrdiff_t remaining_block_size = packet.end().base() - next_block;
        if (remaining_block_size <= 0 ) {
            break;
        }
        if (!rtcp_block.Parse(next_block, remaining_block_size)) {
            if (next_block == packet.begin().base()) {
                PLOG_WARNING << "Incoming invalid RTCP packet";
                return false;
            }
            ++num_skipped_packets_;
            break;
        }

        switch (rtcp_block.type())
        {
        case rtcp::SenderReport::kPacketType:
            if (!ParseSenderReport(rtcp_block)) {
                ++num_skipped_packets_;
            }
            break;
        case rtcp::ReceiverReport::kPacketType:
            break;
        case rtcp::Sdes::kPacketType:
            break;
        case rtcp::RtpFeedback::kPacketType:
            switch (rtcp_block.feedback_message_type())
            {
            case rtcp::Nack::kFeedbackMessageType:
                break;
            default:
                ++num_skipped_packets_;
                break;
            }
            break;
        case rtcp::Psfb::kPacketType:
            switch (rtcp_block.feedback_message_type()) {
            case rtcp::Pli::kFeedbackMessageType:
                break;
            case rtcp::Fir::kFeedbackMessageType:
                break;
            case rtcp::Psfb::kAfbMessageType:
                break;
            default:
                ++num_skipped_packets_;
                break;
            }
            break;
        default:
            ++num_skipped_packets_;
            break;
        }
    }

    if (num_skipped_packets_ > 0) {
    const int64_t now_ms = clock_->TimeInMs();
    if (now_ms - last_skipped_packets_warning_ms_ >= kMaxWarningLogIntervalMs) {
        last_skipped_packets_warning_ms_ = now_ms;
        PLOG_WARNING << num_skipped_packets_
                     << " RTCP blocks were skipped due to being malformed or of "
                        "unrecognized/unsupported type, during the past "
                     << (kMaxWarningLogIntervalMs / 1000) << " second period.";
        }
    }
    return true;
}

bool RtcpReceiver::ParseSenderReport(const rtcp::CommonHeader& rtcp_block) {
    rtcp::SenderReport sender_report;
    if (!sender_report.Parse(rtcp_block)) {
        return false;
    }

    const uint32_t remote_ssrc = sender_report.sender_ssrc();

    // TODO: update tmmbr of remote ssrc is alive

    // We have received RTP packet from this source
    if (remote_ssrc_ == remote_ssrc) {
        // Only signal that we have received a SR when we accept one

        remote_sender_ntp_time_ = sender_report.ntp();
        remote_sender_rtp_time_ = sender_report.rtp_timestamp();
        last_received_sr_ntp_ = clock_->CurrentNtpTime();
        remote_sender_packet_count_ = sender_report.sender_packet_count();
        remote_sender_octet_count_ = sender_report.sender_octet_count();
        remote_sender_reports_count_++;
    }else {
        // We only store one send report from one source,
        // but we will store all the receive blocks
    }

    for (const auto& report_block : sender_report.report_blocks()) {
        HandleReportBlock(report_block, remote_ssrc);
    }

    return true;
}

void RtcpReceiver::HandleReportBlock(const rtcp::ReportBlock& report_block, uint32_t remote_ssrc) {
    // This will be called once per report block in the RTCP packet.
    // We will filter out all report blocks that are not for us.
    // Each packet has max 31 RR blocks.
    //
    // We can calculate RTT if we send a send report and get a report back back.
    // report_block.ssrc() is the SSRC identifier fo the source to which the 
    // information in this reception report block pertains.

    // The source ssrc is in among loca media ssrc|rtx ssrc|fec ssrc
    uint32_t source_ssrc = report_block.source_ssrc();
    // Filter out all report blocks that not for us
    if (!IsRegisteredSsrc(source_ssrc)) {
        return;
    }

    // Update the last time we received an RTCP report block
    last_time_received_rb_ = clock_->CurrentTime();

    // operator[] will create new one if no element with key existed
    auto& report_block_data = received_report_blocks_[source_ssrc];
    
    RTCPReportBlock rtcp_report_block;
    rtcp_report_block.sender_ssrc = remote_ssrc;
    rtcp_report_block.source_ssrc = source_ssrc;
    rtcp_report_block.fraction_lost = report_block.fraction_lost();
    rtcp_report_block.packets_lost = report_block.cumulative_packet_lost();

    // We have successfully delivered new RTP packets to the remote side after
    // the last RR was sent from the remote side.
    if (report_block.extended_high_seq_num() > 
        report_block_data.report_block().extended_highest_sequence_number) {
        last_time_increased_sequence_number_ = last_time_received_rb_;
    }
    rtcp_report_block.extended_highest_sequence_number = report_block.extended_high_seq_num();
    rtcp_report_block.jitter = report_block.jitter();
    rtcp_report_block.delay_since_last_sender_report = report_block.delay_since_last_sr();
    rtcp_report_block.last_sender_report_timestamp = report_block.last_sr_ntp_timestamp();
    report_block_data.SetReportBlock(std::move(rtcp_report_block), utils::time::TimeUTCInMicros());

    int rtt_ms = 0;
    int32_t send_time_ntp = report_block.last_sr_ntp_timestamp();

    // RFC3550, section 6.4.1, LSR field discription states:
    // If no SR has been received yet, the field is set to zero.
    // Receiver observer is not expected to calculate rtt using
    // Sender Reports even if it accidentally can.
    if (send_time_ntp != 0) {
        uint32_t delay_ntp = report_block.delay_since_last_sr();
        // Local NTP time.
        uint32_t receive_time_ntp =
            CompactNtp(clock_->ConvertTimestampToNtpTime(last_time_received_rb_));

        // RTT in 1/(2^16) seconds.
        uint32_t rtt_ntp = receive_time_ntp - delay_ntp - send_time_ntp;
        // Convert to 1/1000 seconds (milliseconds).
        rtt_ms = CompactNtpRttToMs(rtt_ntp);
        report_block_data.AddRoundTripTimeSample(rtt_ms);
        // Only record the RTT from local media source, other than RTX or FEC
        if (report_block.source_ssrc() == local_media_ssrc()) {
            rtts_[remote_ssrc].AddRtt(TimeDelta::Millis(rtt_ms));
        }
    }
}
    
} // namespace naivertc
