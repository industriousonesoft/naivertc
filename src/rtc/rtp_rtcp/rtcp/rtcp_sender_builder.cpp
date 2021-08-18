#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "common/utils_random.hpp"
#include "common/utils_numeric.hpp"
#include "rtc/base/ntp_time_util.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/remb.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/tmmbn.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/tmmbr.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/nack.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/bye.hpp"

#include <plog/Log.h>

namespace naivertc {

// Private methods
void RtcpSender::InitBuilders() {
    builders_[RtcpPacketType::SR] = &RtcpSender::BuildSR;
    builders_[RtcpPacketType::RR] = &RtcpSender::BuildRR;
    builders_[RtcpPacketType::SDES] = &RtcpSender::BuildSDES;
    builders_[RtcpPacketType::PLI] = &RtcpSender::BuildPLI;
    builders_[RtcpPacketType::FIR] = &RtcpSender::BuildFIR;
    builders_[RtcpPacketType::REMB] = &RtcpSender::BuildREMB;
    builders_[RtcpPacketType::BYE] = &RtcpSender::BuildBYE;
    builders_[RtcpPacketType::LOSS_NOTIFICATION] = &RtcpSender::BuildLossNotification;
    builders_[RtcpPacketType::TMMBR] = &RtcpSender::BuildTMMBR;
    builders_[RtcpPacketType::TMMBN] = &RtcpSender::BuildTMMBN;
    builders_[RtcpPacketType::NACK] = &RtcpSender::BuildNACK;
}

bool RtcpSender::ComputeCompoundRtcpPacket(const FeedbackState& feedback_state,
                                            RtcpPacketType rtcp_packt_type,
                                            const std::vector<uint16_t> nack_list,
                                            PacketSender& sender) {
    // Add the flag as volatile. Non volatile entries will not be overwritten.
    // The new volatile flag will be consumed by the end of this call.
    SetFlag(rtcp_packt_type, true);

    // Prevent sending streams to send SR before any media has been sent.
    const bool can_calculate_rtp_timestamp = last_frame_capture_time_.has_value();
    if (!can_calculate_rtp_timestamp) {
        bool consumed_sr_flag = ConsumeFlag(RtcpPacketType::SR);
        bool consumed_report_flag = sending_ && ConsumeFlag(RtcpPacketType::REPORT);
        bool sender_report = consumed_report_flag || consumed_sr_flag;
        // This call was for Sender Report and nothing else.
        if (sender_report && AllVolatileFlagsConsumed()) {
            return true;
        }
        if (sending_) {
            // Not allowed to send any RTCP packet without sender report.
            return false;
        }
    }

    // We need to send out NTP even if we haven't received any reports
    RtcpContext context(feedback_state, nack_list, clock_->CurrentTime());

    PrepareReport(feedback_state);

    bool create_bye = false;
    auto it = report_flags_.begin();
    while (it != report_flags_.end()) {

        RtcpPacketType rtcp_packet_type = it->type;

        if (it->is_volatile) {
            report_flags_.erase(it++);
        } else {
            ++it;
        }

        // If there is a BYE, don't append now - save it and append it
        // at the end later.
        if (rtcp_packet_type == RtcpPacketType::BYE) {
            create_bye = true;
            continue;
        }

        auto builder_it = builders_.find(rtcp_packet_type);
        if (builder_it == builders_.end()) {
            PLOG_WARNING << "Could not find builder for packet type "
                         << uint32_t(rtcp_packet_type);
            continue;
        } else {
            BuilderFunc func = builder_it->second;
            (this->*func)(context, sender);
        }
    }

    // Append the BYE now at the end
    if (create_bye) {
        BuildBYE(context, sender);
    }

    return true;
}

void RtcpSender::PrepareReport(const FeedbackState& feedback_state) {
    // Rtcp Mode: Compund
    if (!IsFlagPresent(RtcpPacketType::SR) && !IsFlagPresent(RtcpPacketType::RR)) {
        SetFlag(sending_ ? RtcpPacketType::SR : RtcpPacketType::RR, true);
    }

    if (IsFlagPresent(RtcpPacketType::SR) || (IsFlagPresent(RtcpPacketType::RR) && !cname_.empty())) {
        SetFlag(RtcpPacketType::SDES, true);
    }

    TimeDelta min_interval = report_interval_;

    // Send video rtcp packets
    if (!audio_ && sending_) {
        // Calculate bandwidth for video
        int send_bitrate_kbit = feedback_state.send_bitrate / 1000;
        if (send_bitrate_kbit != 0) {
            // FIXME: Why ? 360 / send bandwidth in kbit/s.
            min_interval = std::min(TimeDelta::Millis(360000 / send_bitrate_kbit), report_interval_);
        }
    }

    // The interval between RTCP packets is varied randomly over the
    // range [1/2,3/2] times the calculated interval.
    int min_interval_int = utils::numeric::checked_static_cast<int>(min_interval.ms());
    TimeDelta time_to_next = TimeDelta::Millis(utils::random::random(min_interval_int * 1 / 2, min_interval_int * 3 / 2));

    if (time_to_next.IsZero()) {
        PLOG_ERROR << "The interval between RTCP packets is not supposed to be zero.";
        return;
    }

    SetNextRtcpSendEvaluationDuration(time_to_next);

    // RtcpSender expected to be used for sending either just sender reports
    // or just receiver reports.
    assert(!(IsFlagPresent(RtcpPacketType::SR) && IsFlagPresent(RtcpPacketType::RR)));
}

std::vector<rtcp::ReportBlock> RtcpSender::CreateReportBlocks(const FeedbackState& feedback_state) {
    std::vector<rtcp::ReportBlock> report_blocks;
    
    // TODO: Retrive report blocks from Receiver statistics

    // How to calculate RTT: https://blog.jianchihu.net/webrtc-research-stats-rtt.html
    // Receiver          Network         Sender
    //     |---------->                     |
    //     |           ----RR---->          |
    //     |                       -------->| t0 (last_rr)
    //     |                                |     | delay_since_last_sr (for sender)
    //     |                       <--------| t1 (new_sr)
    //     |           <----SR----          |
    //     |<----------                     |
    //     |                                |
    if (!report_blocks.empty() && (feedback_state.last_rr_ntp_secs !=0 || feedback_state.last_rr_ntp_frac != 0)) {
        // Get our NTP as late as possible to avoid a race.
        uint32_t now = CompactNtp(clock_->CurrentNtpTime());

        // Convert 64-bits NTP time to 32-bits(compact) NTP
        uint32_t receive_time = feedback_state.last_rr_ntp_secs & 0x0000FFFF;
        receive_time <<= 16;
        receive_time += (feedback_state.last_rr_ntp_frac & 0xffff0000) >> 16;

        // Delay since the last RR
        uint32_t delay_since_last_sr = now - receive_time;

        for (auto& report_block : report_blocks) {
            report_block.set_last_sr_ntp_timestamp(feedback_state.remote_sr);
            report_block.set_delay_sr_since_last_sr(delay_since_last_sr);
        }
    }

    return report_blocks;
}

void RtcpSender::BuildSR(const RtcpContext& ctx, PacketSender& sender) {
    if (!last_frame_capture_time_.has_value()) {
        PLOG_WARNING << "RTCP SR shouldn't be built before first media frame.";
        return;
    }
    // The timestamp of this RTCP packet should be estimated as the timestamp of
    // the frame being captured at this moment. We are calculating that
    // timestamp as the last frame's timestamp + the time since the last frame
    // was captured.
    int rtp_rate = rtp_clock_rates_khz_[last_rtp_payload_type_];
    if (rtp_rate <= 0) {
        rtp_rate = (audio_ ? kBogusRtpRateForAudioRtcp : kVideoPayloadTypeFrequency) / 1000;
    }

    // Round now_us_ to the closest millisecond, because Ntp time is rounded
    // when converted to milliseconds,
    uint32_t rtp_timestamp = timestamp_offset_ + last_rtp_timestamp_ +
                ((ctx.now_.us() + 500) / 1000 - last_frame_capture_time_->ms()) * rtp_rate;

    PLOG_INFO << "timestamp_offset: " << timestamp_offset_ 
              << " last_rtp_timestamp: " << last_rtp_timestamp_ 
              << "rtp_timestamp: " << rtp_timestamp;

    rtcp::SenderReport sr;
    sr.set_sender_ssrc(ssrc_);
    sr.set_ntp(clock_->ConvertTimestampToNtpTime(ctx.now_));
    sr.set_rtp_timestamp(rtp_timestamp);
    sr.set_sender_packet_count(ctx.feedback_state_.packets_sent);
    sr.set_sender_octet_count(ctx.feedback_state_.media_bytes_sent);
    sr.SetReportBlocks(CreateReportBlocks(ctx.feedback_state_));
    sender.AppendPacket(sr);
}

void RtcpSender::BuildRR(const RtcpContext& ctx, PacketSender& sender) {
    rtcp::ReceiverReport rr;
    rr.set_sender_ssrc(ssrc_);
    rr.SetReportBlocks(CreateReportBlocks(ctx.feedback_state_));
    sender.AppendPacket(rr);
}

void RtcpSender::BuildSDES(const RtcpContext& ctx, PacketSender& sender) {
    rtcp::Sdes sdes;
    sdes.AddCName(ssrc_, cname_);
    sender.AppendPacket(sdes);
}

void RtcpSender::BuildFIR(const RtcpContext& ctx, PacketSender& sender) {

}

void RtcpSender::BuildPLI(const RtcpContext& ctx, PacketSender& sender) {

}

void RtcpSender::BuildREMB(const RtcpContext& ctx, PacketSender& sender) {
    rtcp::Remb remb;
    remb.set_sender_ssrc(ssrc_);
    remb.set_bitrate_bps(remb_bitrate_);
    remb.set_ssrcs(remb_ssrcs_);
    sender.AppendPacket(remb);
}

void RtcpSender::BuildTMMBR(const RtcpContext& ctx, PacketSender& sender) {

}

void RtcpSender::BuildTMMBN(const RtcpContext& ctx, PacketSender& sender) {

}

void RtcpSender::BuildLossNotification(const RtcpContext& ctx, PacketSender& sender) {
    loss_notification_.set_sender_ssrc(ssrc_);
    loss_notification_.set_media_ssrc(remote_ssrc_);
    sender.AppendPacket(loss_notification_);
}

void RtcpSender::BuildNACK(const RtcpContext& ctx, PacketSender& sender) {
    rtcp::Nack nack;
    nack.set_sender_ssrc(ssrc_);
    nack.set_media_ssrc(remote_ssrc_);
    nack.set_packet_ids(ctx.nack_list_);

    for (uint16_t id : ctx.nack_list_) {
        nack_stats_.ReportRequest(id);
    }

    sender.AppendPacket(nack);
}

void RtcpSender::BuildBYE(const RtcpContext& ctx, PacketSender& sender) {
    rtcp::Bye bye;
    bye.set_sender_ssrc(ssrc_);
    bye.set_csrcs(csrcs_);
    sender.AppendPacket(bye);
}

} // namespace naivertc
