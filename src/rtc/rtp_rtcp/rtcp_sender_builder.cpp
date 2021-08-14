#include "rtc/rtp_rtcp/rtcp_sender.hpp"
#include "common/utils_random.hpp"
#include "common/utils_numeric.hpp"
#include "rtc/rtp_rtcp/time_util.hpp"

#include <plog/Log.h>

namespace naivertc {

// Private methods
std::optional<int32_t> RtcpSender::ComputeCompoundRtcpPacket(
        const FeedbackState& feedback_state,
        RtcpPacketType rtcp_packt_type,
        int32_t nack_size,
        const uint16_t* nack_list,
        PacketSender& sender) {
    // Add the flag as volatile. Non volatile entries will not be overwritten.
    // The new volatile flag will be consumed by the end of this call.
    SetFlag(rtcp_packt_type, true);

    // Prevent sending streams to send SR before any media has been sent.
    const bool can_calculate_rtp_timestamp = last_frame_capture_time_.has_value();
    if (!can_calculate_rtp_timestamp) {
        bool consumed_sr_flag = ConsumeFlag(kRtcpSr);
        bool consumed_report_flag = sending_ && ConsumeFlag(kRtcpReport);
        bool sender_report = consumed_report_flag || consumed_sr_flag;
        // This call was for Sender Report and nothing else.
        if (sender_report && AllVolatileFlagsConsumed()) {
            return 0;
        }
        if (sending_) {
            // Not allowed to send any RTCP packet without sender report.
            return -1;
        }
    }

    // We need to send out NTP even if we haven't received any reports
    RtcpContext context(feedback_state, nack_size, nack_list, clock_->CurrentTime());

    PrepareReport(feedback_state);

    bool create_bye = false;
    auto it = report_flags_.begin();
    while (it != report_flags_.end()) {

        uint32_t rtcp_packet_type = it->type;

        if (it->is_volatile) {
            report_flags_.erase(it++);
        } else {
            ++it;
        }

        // If there is a BYE, don't append now - save it and append it
        // at the end later.
        if (rtcp_packet_type == kRtcpBye) {
            create_bye = true;
            continue;
        }

        // TODO: Build RTCP packet for type
        // auto builder_it = builders_.find(rtcp_packet_type);
        // if (builder_it == builders_.end()) {
        //     PLOG_WARNING << "Could not find builder for packet type "
        //                  << rtcp_packet_type;
        //     continue;
        // } else {
        //     BuilderFunc func = builder_it->second;
        //     (this->*func)(context, sender);
        // }
    }

    // Append the BYE now at the end
    if (create_bye) {
        // TODO: Built Bye packet
        // BuildBYE(context, sender);
    }

    return std::nullopt;
}

void RtcpSender::PrepareReport(const FeedbackState& feedback_state) {
    // Rtcp Mode: Compund
    if (!IsFlagPresent(kRtcpSr) && !IsFlagPresent(kRtcpRr)) {
        SetFlag(sending_ ? kRtcpSr : kRtcpRr, true);
    }

    if (IsFlagPresent(kRtcpSr) || (IsFlagPresent(kRtcpRr) && !cname_.empty())) {
        SetFlag(kRtcpSdes, true);
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
    assert(!(IsFlagPresent(kRtcpSr) && IsFlagPresent(kRtcpRr)));
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
    
} // namespace naivertc
