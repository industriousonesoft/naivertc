#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "common/utils_time.hpp"
#include "rtc/base/time/ntp_time_util.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr int64_t kMaxWarningLogIntervalMs = 10000;

// Maximum number of received RRTRs that will be stored.
constexpr size_t kMaxNumberOfStoredRrtrs = 300;
    
} // namespace


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

        if (packet_type_counter_.first_packet_time_ms == -1) {
            packet_type_counter_.first_packet_time_ms = clock_->now_ms();
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
        // bye
        case rtcp::Bye::kPacketType:
            if (!ParseBye(rtcp_block)) {
                ++num_skipped_packets_;
            }
            break;
        case rtcp::ExtendedReports::kPacketType:
            if (!ParseXr(rtcp_block, packet_info)) {
                ++num_skipped_packets_;
            }
            break;
        // Rtp feedback
        case rtcp::Rtpfb::kPacketType:
            switch (rtcp_block.feedback_message_type())
            {
            case rtcp::Nack::kFeedbackMessageType:
                if (!ParseNack(rtcp_block, packet_info)) {
                    ++num_skipped_packets_;
                }
                break;
            case rtcp::Tmmbr::kFeedbackMessageType:
                ++num_skipped_packets_;
                break;
            case rtcp::Tmmbn::kFeedbackMessageType:
                ++num_skipped_packets_;
                break;
            case rtcp::TransportFeedback::kFeedbackMessageType:
                if (!ParseTransportFeedback(rtcp_block, packet_info)) {
                    ++num_skipped_packets_;
                }
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
                if (!ParsePli(rtcp_block, packet_info)) {
                    ++num_skipped_packets_;
                }
                break;
            case rtcp::Fir::kFeedbackMessageType:
                if (!ParseFir(rtcp_block, packet_info)) {
                    ++num_skipped_packets_;
                }
                break;
            case rtcp::Psfb::kAfbMessageType:
                if (!ParseAfb(rtcp_block, packet_info)) {
                    ++num_skipped_packets_;
                }
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

    if (packet_type_counter_observer_) {
        packet_type_counter_observer_->RtcpPacketTypesCounterUpdated(local_media_ssrc(), packet_type_counter_);
    }

    if (num_skipped_packets_ > 0) {
        const int64_t now_ms = clock_->now_ms();
        if (now_ms - last_skipped_packets_warning_ms_ >= kMaxWarningLogIntervalMs) {
            last_skipped_packets_warning_ms_ = now_ms;
            PLOG_WARNING << num_skipped_packets_
                         << " RTCP blocks were skipped due to being malformed or of"
                            " unrecognized/unsupported type, during the past "
                         << (kMaxWarningLogIntervalMs / 1000) 
                         << " second period.";
            }
    }
    return true;
}

// Sr
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
        packet_info->packet_type_flags |= RtcpPacketType::SR;

        last_sr_stats_.send_ntp_time = sender_report.ntp();
        last_sr_stats_.send_rtp_time = sender_report.rtp_timestamp();
        last_sr_stats_.arrival_ntp_time = clock_->CurrentNtpTime();
        last_sr_stats_.packet_sent = sender_report.sender_packet_count();
        last_sr_stats_.bytes_sent = sender_report.sender_octet_count();
        last_sr_stats_.reports_count += 1;
    } else {
        // We only store one send report from one source,
        // but we will store all the receive blocks
        packet_info->packet_type_flags |= RtcpPacketType::RR;
    }

    // Parse all report blocks of the send report.
    for (const auto& report_block : sender_report.report_blocks()) {
        HandleReportBlock(report_block, packet_info, remote_ssrc);
    }

    return true;
}

// Rr
bool RtcpReceiver::ParseReceiverReport(const rtcp::CommonHeader& rtcp_block, 
                                       PacketInfo* packet_info) {
    rtcp::ReceiverReport receiver_report;
    if (!receiver_report.Parse(rtcp_block)) {
        return false;
    }

    const uint32_t remote_ssrc = receiver_report.sender_ssrc();

    packet_info->remote_ssrc = remote_ssrc;
    packet_info->packet_type_flags |= RtcpPacketType::RR;

    // TODO: update tmmbr of remote if it's alive

    // Parse all report blocks of the receive report.
    for (const auto& report_block : receiver_report.report_blocks()) {
        HandleReportBlock(report_block, packet_info, remote_ssrc);
    }

    return true;
}

// Report block
void RtcpReceiver::HandleReportBlock(const rtcp::ReportBlock& report_block, 
                                     PacketInfo* packet_info, 
                                     uint32_t remote_ssrc) {
    // This will be called once per report block in the RTCP packet.
    // We will filter out all report blocks that are not for us.
    // Each packet has max 31 RR blocks.
    //
    // We can calculate RTT if we send a send report and get a report back back.
    //
    // `report_block.source_ssrc()` is the SSRC identifier of the source to which the 
    // information in this reception report block pertains.

    // The source ssrc is one of [loca media ssrc|rtx ssrc|fec ssrc].
    uint32_t source_ssrc = report_block.source_ssrc();
    // Filter out all report blocks that not for us
    if (!IsRegisteredSsrc(source_ssrc)) {
        return;
    }

    // Update the last time we received an RTCP report block
    last_time_received_rb_ = clock_->CurrentTime();

    RtcpReportBlock* rtcp_report_block = &received_report_blocks_[source_ssrc];

    rtcp_report_block->sender_ssrc = remote_ssrc;
    rtcp_report_block->source_ssrc = source_ssrc;
    rtcp_report_block->fraction_lost = report_block.fraction_lost();
    rtcp_report_block->packets_lost = report_block.cumulative_packet_lost();
    // We have successfully delivered new RTP packets to the remote side after
    // the last RR was sent from the remote side.
    if (report_block.extended_high_seq_num() > rtcp_report_block->extended_highest_sequence_number) {
        last_time_increased_sequence_number_ = last_time_received_rb_;
    }
    rtcp_report_block->extended_highest_sequence_number = report_block.extended_high_seq_num();
    rtcp_report_block->jitter = report_block.jitter();
    rtcp_report_block->delay_since_last_sender_report = report_block.delay_since_last_sr();
    rtcp_report_block->last_sender_report_timestamp = report_block.last_sr_ntp_timestamp();

    int rtt_ms = 0;
    uint32_t send_time_ntp = report_block.last_sr_ntp_timestamp();

    // RFC3550, section 6.4.1, LSR field discription states:
    // If no SR has been received yet, the field is set to zero.
    // Receiver observer is not expected to calculate rtt using
    // Sender Reports even if it accidentally can.
    if (send_time_ntp != 0) {
        uint32_t delay_ntp = report_block.delay_since_last_sr();
        // Local NTP time.
        uint32_t receive_time_ntp = CompactNtp(clock_->ConvertTimestampToNtpTime(last_time_received_rb_));

        // RTT in 1/(2^16) seconds.
        uint32_t rtt_ntp = receive_time_ntp - delay_ntp - send_time_ntp;
        // Convert to 1/1000 seconds (milliseconds).
        rtt_ms = CompactNtpRttToMs(rtt_ntp);
        rtts_[source_ssrc].AddRttMs(TimeDelta::Millis(rtt_ms));
        // FIXME: Only record the RTT from local media source, other than RTX or FEC?
        if (source_ssrc == local_media_ssrc()) {
            rtts_[remote_ssrc].AddRttMs(TimeDelta::Millis(rtt_ms));
        }
        packet_info->rtt_ms = rtt_ms;
    }

    packet_info->report_blocks.push_back(*rtcp_report_block);
}

// Sdes
bool RtcpReceiver::ParseSdes(const rtcp::CommonHeader& rtcp_block,
                             PacketInfo* packet_info) {
    rtcp::Sdes sdes;
    if (!sdes.Parse(rtcp_block)) {
        return false;
    }
    for (const auto& chunk : sdes.chunks()) {
        PLOG_VERBOSE << "Received: ssrc=" << chunk.ssrc
                     << ", cname=" << chunk.cname;
        if (cname_observer_) {
            cname_observer_->OnCname(chunk.ssrc, chunk.cname);
        }
    }
    packet_info->packet_type_flags |= RtcpPacketType::SDES;
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
        return false;
    }

    packet_info->nack_list.insert(packet_info->nack_list.end(), 
                                  nack.packet_ids().begin(), 
                                  nack.packet_ids().end());

    for (uint16_t packet_id : nack.packet_ids()) {
        nack_stats_.ReportRequest(packet_id);
    }

    if (!nack.packet_ids().empty()) {
        packet_info->packet_type_flags |= RtcpPacketType::NACK;
        ++packet_type_counter_.nack_packets;
        packet_type_counter_.nack_requests = nack_stats_.requests();
        packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();
    }

    return true;
}

bool RtcpReceiver::ParseTransportFeedback(const rtcp::CommonHeader& rtcp_block, 
                                          PacketInfo* packet_info) {
    rtcp::TransportFeedback transport_feedback;
    if (!transport_feedback.Parse(rtcp_block)) {
        return false;
    }

    packet_info->packet_type_flags |= RtcpPacketType::TRANSPORT_FEEDBACK;
    if (transport_feedback_observer_) {
        uint32_t media_source_ssrc = transport_feedback.media_ssrc();
        if (IsRegisteredSsrc(media_source_ssrc)) {
            transport_feedback_observer_->OnTransportFeedback(transport_feedback);
        }
    }
    return true;
}

// Pli
bool RtcpReceiver::ParsePli(const rtcp::CommonHeader& rtcp_block,
                            PacketInfo* packet_info) {
    rtcp::Pli pli;
    if (!pli.Parse(rtcp_block)) {
        return false;
    }

    if (local_media_ssrc() == pli.media_ssrc()) {
        ++packet_type_counter_.pli_packets;
        // Received a signal that we need to send a new key frame.
        packet_info->packet_type_flags |= RtcpPacketType::PLI;

        PLOG_VERBOSE << "Received FLI from remote ssrc=" << packet_info->remote_ssrc;
    }

    return true;
}

// Fir
bool RtcpReceiver::ParseFir(const rtcp::CommonHeader& rtcp_block,
                            PacketInfo* packet_info) {
    rtcp::Fir fir;
    if (!fir.Parse(rtcp_block)) {
        return false;
    }

    if (fir.requests().empty()) {
        return false;
    }

    const uint32_t media_ssrc = local_media_ssrc();
    for (const auto& fir_request : fir.requests()) {
        // Filter the requests that not belong our sender.
        if (media_ssrc != fir_request.ssrc) {
            continue;
        }

        ++packet_type_counter_.fir_packets;

        // Received a signal that we need to send a new key frame.
        packet_info->packet_type_flags |= RtcpPacketType::FIR;
    }

    PLOG_VERBOSE << "Received FIR from remote ssrc=" << packet_info->remote_ssrc;

    return true;
}

// Afb (Application feedback)
bool RtcpReceiver::ParseAfb(const rtcp::CommonHeader& rtcp_block,
                            PacketInfo* packet_info) {
    {
        rtcp::Remb remb;
        if (remb.Parse(rtcp_block)) {
            packet_info->packet_type_flags |= RtcpPacketType::REMB;
            packet_info->remb_bps = remb.bitrate_bps();
            return true;
        }
    }

    {
        rtcp::LossNotification loss_notification;
        if (loss_notification.Parse(rtcp_block)) {
            packet_info->packet_type_flags |= RtcpPacketType::LOSS_NOTIFICATION;
            if (loss_notification_observer_ && loss_notification.media_ssrc() == local_media_ssrc()) {
                loss_notification_observer_->OnReceivedLossNotification(loss_notification.media_ssrc(),
                                                                        loss_notification.last_decoded(),
                                                                        loss_notification.last_received(),
                                                                        loss_notification.decodability_flag());
            }
            return true;
        }
    }

    PLOG_WARNING << "Unknown PSFB-APP packet.";

    return false;
}

// Bye 
bool RtcpReceiver::ParseBye(const rtcp::CommonHeader& rtcp_block) {
    rtcp::Bye bye;
    if (!bye.Parse(rtcp_block)) {
        return false;
    }

    // Clear our lists
    // Report blocks
    rtts_.erase(bye.sender_ssrc());
    for (auto it = received_report_blocks_.begin(); it != received_report_blocks_.end();) {
        if (it->second.sender_ssrc == bye.sender_ssrc()) {
            it = received_report_blocks_.erase(it);
        } else {
            ++it;
        }
    }

    // Rrtr
    auto it = rrtr_its_.find(bye.sender_ssrc());
    if (it != rrtr_its_.end()) {
        rrtrs_.erase(it->second);
        rrtr_its_.erase(it);
    }
    xr_rr_rtt_ms_ = 0;
    return true;
}

// Xr
bool RtcpReceiver::ParseXr(const rtcp::CommonHeader& rtcp_block, 
                           PacketInfo* packet_info) {
    rtcp::ExtendedReports xr;
    if (!xr.Parse(rtcp_block)) {
        return false;
    }

    // Rrtr
    if (xr.rrtr()) {
        HandleXrRrtrBlock(*xr.rrtr(), xr.sender_ssrc());
    }
    // Dlrr
    HandleXrDlrrBlock(xr.dlrr());
    // TargetBitrate
    if (xr.target_bitrate()) {
        HandleXrTargetBitrateBlock(*xr.target_bitrate(), packet_info, xr.sender_ssrc());
    }

    return true;
}

// Rrtr: Receiver Reference Time Report block 
void RtcpReceiver::HandleXrRrtrBlock(const rtcp::Rrtr& rrtr, uint32_t sender_ssrc) {
    uint32_t received_remote_mid_ntp_time = CompactNtp(rrtr.ntp());
    uint32_t local_receive_mid_ntp_time = CompactNtp(clock_->CurrentNtpTime());

    auto it = rrtr_its_.find(sender_ssrc);
    if (it != rrtr_its_.end()) {
        it->second->received_remote_mid_ntp_time = received_remote_mid_ntp_time;
        it->second->local_receive_mid_ntp_time = local_receive_mid_ntp_time;
    } else {
        if (rrtrs_.size() < kMaxNumberOfStoredRrtrs) {
            rrtrs_.emplace_back(sender_ssrc, received_remote_mid_ntp_time, local_receive_mid_ntp_time);
            rrtr_its_[sender_ssrc] = std::prev(rrtrs_.end());
        } else {
            PLOG_WARNING << "Reached the maximum number of storted RRTRs, ignoring.";
        }
    }
}

void RtcpReceiver::HandleXrDlrrBlock(const rtcp::Dlrr& dlrr) {
    if (dlrr.sub_blocks().size()) {
        return;
    }
    for (auto& sub_block : dlrr.sub_blocks()) {
        if (!IsRegisteredSsrc(sub_block.ssrc)) {
            // Not to us.
            continue;
        }
        // The send_time and delay_rr fields are in units of 1/2^16 sec.
        uint32_t send_time_ntp = sub_block.last_rr;
        // RFC3611, section 4.5, LRR field discription states:
        // If no such block has been received, the field is set to zero.
        if (send_time_ntp == 0) {
            continue;
        }
        uint32_t delay_ntp = sub_block.delay_since_last_rr;
        uint32_t now_ntp = CompactNtp(clock_->CurrentNtpTime());

        uint32_t rtt_ntp = now_ntp - delay_ntp - send_time_ntp;
        xr_rr_rtt_ms_ = CompactNtpRttToMs(rtt_ntp);
    }
}

void RtcpReceiver::HandleXrTargetBitrateBlock(const rtcp::TargetBitrate& target_birate,
                                              PacketInfo* packet_info, 
                                              uint32_t ssrc) {
    if (ssrc != remote_ssrc_) {
        return;
    }
    // VideoBitrateAllocation
}
    
} // namespace naivertc
