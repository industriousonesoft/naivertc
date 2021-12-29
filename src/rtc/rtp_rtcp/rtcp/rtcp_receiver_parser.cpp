#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/rtp_feedback.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/nack.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/psfb.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/tmmbr.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/tmmbn.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/bye.hpp"

#include "common/utils_time.hpp"
#include "rtc/base/time/ntp_time_util.hpp"
#include <plog/Log.h>

namespace naivertc {

constexpr int64_t kMaxWarningLogIntervalMs = 10000;

// Compound packet
bool RtcpReceiver::ParseCompoundPacket(CopyOnWriteBuffer packet, 
                                       PacketInfo* packet_info) {

    rtcp::CommonHeader rtcp_block;
    for (const uint8_t* next_block = packet.begin().base(); next_block != packet.end().base(); 
        next_block = rtcp_block.NextPacket()) {
        ptrdiff_t remaining_block_size = packet.end().base() - next_block;
        if (remaining_block_size <= 0 ) {
            break;
        }
        // Parse the next RTCP packet.
        if (!rtcp_block.Parse(next_block, remaining_block_size)) {
            // Failed to parse the first RTCP header, noting was extracted from this compound packet.
            if (next_block == packet.begin().base()) {
                PLOG_WARNING << "Incoming invalid RTCP packet.";
                return false;
            }
            ++num_skipped_packets_;
            break;
        }

        switch (rtcp_block.type())
        {
        // Sender report
        case rtcp::SenderReport::kPacketType:
            if (!ParseSenderReport(rtcp_block, packet_info)) {
                ++num_skipped_packets_;
            }
            break;
        // Receiver report
        case rtcp::ReceiverReport::kPacketType:
            if (!ParseReceiverReport(rtcp_block, packet_info)) {
                ++num_skipped_packets_;
            }
            break;
        // Sdes 
        case rtcp::Sdes::kPacketType:
            if (!ParseSdes(rtcp_block, packet_info)) {
                ++num_skipped_packets_;
            }
            break;
        // Rtp feedback
        case rtcp::RtpFeedback::kPacketType:
            switch (rtcp_block.feedback_message_type())
            {
            case rtcp::Nack::kFeedbackMessageType:
                if (!ParseNack(rtcp_block, packet_info)) {
                    ++num_skipped_packets_;
                }
                break;
            case rtcp::Tmmbr::kFeedbackMessageType:
                break;
            case rtcp::Tmmbn::kFeedbackMessageType:
                break;
            default:
                ++num_skipped_packets_;
                break;
            }
            break;
        // Payload-specific feedback
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
        const int64_t now_ms = clock_->now_ms();
        if (now_ms - last_skipped_packets_warning_ms_ >= kMaxWarningLogIntervalMs) {
            last_skipped_packets_warning_ms_ = now_ms;
            PLOG_WARNING << num_skipped_packets_
                         << " RTCP blocks were skipped due to being malformed or of "
                            "unrecognized/unsupported type, during the past "
                         << (kMaxWarningLogIntervalMs / 1000) 
                         << " second period.";
            }
    }
    return true;
}

// Sender report packet
bool RtcpReceiver::ParseSenderReport(const rtcp::CommonHeader& rtcp_block, 
                                     PacketInfo* packet_info) {
    rtcp::SenderReport sender_report;
    if (!sender_report.Parse(rtcp_block)) {
        return false;
    }

    // Remote media ssrc
    const uint32_t remote_ssrc = sender_report.sender_ssrc();

    packet_info->remote_ssrc = remote_ssrc;

    // TODO: update tmmbr of remote if it's alive

    // Accept the send report if we have received RTP packet 
    // from the same media source.
    if (remote_ssrc_ == remote_ssrc) {
        // Only signal that we have received a SR when we accept one
        packet_info->packet_type_flags |= rtcp::SenderReport::kPacketType;

        remote_sender_ntp_time_ = sender_report.ntp();
        remote_sender_rtp_time_ = sender_report.rtp_timestamp();
        last_received_sr_ntp_ = clock_->CurrentNtpTime();
        remote_sender_packet_count_ = sender_report.sender_packet_count();
        remote_sender_octet_count_ = sender_report.sender_octet_count();
        remote_sender_reports_count_++;
    } else {
        // We only store one send report from one source,
        // but we will store all the receive blocks
        packet_info->packet_type_flags |= rtcp::ReceiverReport::kPacketType;
    }

    // Parse all report blocks of the send report.
    for (const auto& report_block : sender_report.report_blocks()) {
        ParseReportBlock(report_block, packet_info, remote_ssrc);
    }

    return true;
}

bool RtcpReceiver::ParseReceiverReport(const rtcp::CommonHeader& rtcp_block, 
                                       PacketInfo* packet_info) {
    rtcp::ReceiverReport receiver_report;
    if (!receiver_report.Parse(rtcp_block)) {
        return false;
    }

    const uint32_t remote_ssrc = receiver_report.sender_ssrc();

    packet_info->remote_ssrc = remote_ssrc;
    packet_info->packet_type_flags |= rtcp::ReceiverReport::kPacketType;

    // TODO: update tmmbr of remote if it's alive

    // Parse all report blocks of the receive report.
    for (const auto& report_block : receiver_report.report_blocks()) {
        ParseReportBlock(report_block, packet_info, remote_ssrc);
    }

    return true;
}

void RtcpReceiver::ParseReportBlock(const rtcp::ReportBlock& report_block, 
                                    PacketInfo* packet_info, 
                                    uint32_t remote_ssrc) {
    // This will be called once per report block in the RTCP packet.
    // We will filter out all report blocks that are not for us.
    // Each packet has max 31 RR blocks.
    //
    // We can calculate RTT if we send a send report and get a report back back.
    //
    // `report_block.source_ssrc()` is the SSRC identifier fo the source to which the 
    // information in this reception report block pertains.

    // The source ssrc is one of [loca media ssrc|rtx ssrc|fec ssrc].
    uint32_t source_ssrc = report_block.source_ssrc();
    // Filter out all report blocks that not for us
    if (!IsRegisteredSsrc(source_ssrc)) {
        return;
    }

    // Update the last time we received an RTCP report block
    last_received_rb_ = clock_->CurrentTime();

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
        last_time_increased_sequence_number_ = last_received_rb_;
    }
    rtcp_report_block.extended_highest_sequence_number = report_block.extended_high_seq_num();
    rtcp_report_block.jitter = report_block.jitter();
    rtcp_report_block.delay_since_last_sender_report = report_block.delay_since_last_sr();
    rtcp_report_block.last_sender_report_timestamp = report_block.last_sr_ntp_timestamp();
    report_block_data.SetReportBlock(std::move(rtcp_report_block), utils::time::TimeUTCInMicros());

    int rtt_ms = 0;
    uint32_t send_time_ntp = report_block.last_sr_ntp_timestamp();

    // RFC3550, section 6.4.1, LSR field discription states:
    // If no SR has been received yet, the field is set to zero.
    // Receiver observer is not expected to calculate rtt using
    // Sender Reports even if it accidentally can.
    if (send_time_ntp != 0) {
        uint32_t delay_ntp = report_block.delay_since_last_sr();
        // Local NTP time.
        uint32_t receive_time_ntp = CompactNtp(clock_->ConvertTimestampToNtpTime(last_received_rb_));

        // RTT in 1/(2^16) seconds.
        uint32_t rtt_ntp = receive_time_ntp - delay_ntp - send_time_ntp;
        // Convert to 1/1000 seconds (milliseconds).
        rtt_ms = CompactNtpRttToMs(rtt_ntp);
        report_block_data.AddRttMs(rtt_ms);
        // Only record the RTT from local media source, other than RTX or FEC
        if (report_block.source_ssrc() == local_media_ssrc()) {
            rtts_[remote_ssrc].AddRtt(TimeDelta::Millis(rtt_ms));
        }
    }
}

// Sdes
bool RtcpReceiver::ParseSdes(const rtcp::CommonHeader& rtcp_block,
                             PacketInfo* packet_info) {
    rtcp::Sdes sdes;
    if (!sdes.Parse(rtcp_block)) {
        return false;
    }
    for (const auto& chunk : sdes.chunks()) {
        // TODO: cname callback
    }
    return true;
}

// Nack 
bool RtcpReceiver::ParseNack(const rtcp::CommonHeader& rtcp_block,
                             PacketInfo* packet_info) {
    rtcp::Nack nack;
    if (!nack.Parse(rtcp_block)) {
        return false;
    }

    // Not to us but return true
    if (receiver_only_ || local_media_ssrc() != nack.media_ssrc()) {
        return true;
    }

    for (uint16_t packet_id : nack.packet_ids()) {
        nack_stats_.ReportRequest(packet_id);
    }

    if (!nack.packet_ids().empty()) {
        // TODO: packet type counter
    }

    return true;
}

// Bye 
bool RtcpReceiver::ParseBye(const rtcp::CommonHeader& rtcp_block,
                            PacketInfo* packet_info) {
    rtcp::Bye bye;
    if (bye.Parse(rtcp_block)) {
        return false;
    }

    // Clear our lists
    rtts_.erase(bye.sender_ssrc());
    for (auto it = received_report_blocks_.begin(); it != received_report_blocks_.end(); ++it) {
        if (it->second.report_block().sender_ssrc == bye.sender_ssrc()) {
            received_report_blocks_.erase(it);
            break;
        }
    }
    return true;
}
    
} // namespace naivertc
