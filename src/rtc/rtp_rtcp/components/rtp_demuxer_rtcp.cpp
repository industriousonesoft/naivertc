#include "rtc/rtp_rtcp/components/rtp_demuxer.hpp"
#include "rtc/rtp_rtcp/base/rtp_utils.hpp"

#include <plog/Log.h>

#include <set>

namespace naivertc {
namespace {

// RTCP common header
struct RTC_CPP_EXPORT RtcpHeader {
    uint8_t first_byte;
    uint8_t payload_type;
    uint16_t payload_size_in_32bit;
};

// RTCP report block
struct RTC_CPP_EXPORT ReportBlock {
    uint32_t source_ssrc;
    uint32_t fraction_lost_and_packet_lost;
    uint16_t seq_num_cycles;
    uint16_t highest_seq_num;
    uint32_t jitter;
    uint32_t last_report;
    uint32_t delay_since_last_report;
};

// RTCP sender report packet (payload type = 200)
struct RTC_CPP_EXPORT SenderReport {
    RtcpHeader header;
    uint32_t packet_sender_ssrc;
    uint64_t ntp_timestamp;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;

    ReportBlock report_blocks;

    uint8_t report_count() const { return header.first_byte & 0x1F; }
    const ReportBlock* GetReportBlock(uint8_t num) const { 
        return &report_blocks + sizeof(ReportBlock) * num; 
    }

};

// RTCP receiver report packet (payload type = 201)
struct RTC_CPP_EXPORT ReceiverReport {
    RtcpHeader header;
    uint32_t packet_sender_ssrc;

    ReportBlock report_blocks;

    uint8_t report_count() const { return header.first_byte & 0x1F; }
    const ReportBlock* GetReportBlock(uint8_t num) const { 
        return &report_blocks + sizeof(ReportBlock) * num; 
    }
};

// Bye (payload type = 203)
struct RTC_CPP_EXPORT Bye {
    RtcpHeader header;
    uint32_t packet_sender_ssrc;
};

// RTCP rtp feedback packet (payload type = 205)
struct RTC_CPP_EXPORT RtpFeedback {
    RtcpHeader header;
    uint32_t packet_sender_ssrc;
    uint32_t media_source_ssrc;
};

// RTCP payload-specific feedback packet (payload type = 206)
struct RTC_CPP_EXPORT PSFeedback {
    RtcpHeader header;
    uint32_t packet_sender_ssrc;
    uint32_t media_source_ssrc;
};

// RTCP extended report packet (payload type = 207)
struct RTC_CPP_EXPORT ExtendedReport {
    RtcpHeader header;
    uint32_t packet_sender_ssrc;
};
    
} // namespace

bool RtpDemuxer::DeliverRtcpPacket(CopyOnWriteBuffer in_packet) const {
    if (rtcp_sink_by_ssrc_.empty()) {
        PLOG_WARNING << "No RTCP sink available.";
        return false;
    }
    if (!IsRtcpPacket(in_packet)) {
        PLOG_WARNING << "The incoming packet is not a RTCP packet.";
        return false;
    }
    std::set<uint32_t> ssrcs;
    size_t packet_size = in_packet.size();
    size_t offset = 0;
    // Indicate if the compound packet will deliver to all sinks or not.
    bool deliver_to_all = false;
    // Parse buffer as a compound packet
    while((offset + 4 /* RTCP header fixed size */) <= packet_size) {
        auto rtcp_header = reinterpret_cast<RtcpHeader *>(in_packet.data() + offset);
        // Calculate packet size in bytes
        size_t rtcp_packet_size = /* rtcp_header_fixed_size=*/ 4 + /*payload_size=*/ntohs(rtcp_header->payload_size_in_32bit) * 4;
        if (rtcp_packet_size > packet_size - offset) {
            break;
        }
        offset += rtcp_packet_size;
        // RTCP sender report (pt = 200)
        if (rtcp_header->payload_type == 200) {
            auto rtcp_sr = reinterpret_cast<SenderReport*>(rtcp_header);
            uint32_t sender_ssrc = ntohl(rtcp_sr->packet_sender_ssrc);
            ssrcs.insert(sender_ssrc);
            for (uint8_t i = 0; i < rtcp_sr->report_count(); i++) {
                uint32_t media_source_ssrc = ntohl(rtcp_sr->GetReportBlock(i)->source_ssrc);
                ssrcs.insert(media_source_ssrc);
            }
        } 
        // RTCP receiver report (pt = 201)
        else if (rtcp_header->payload_type == 201) {
            auto rtcp_sr = reinterpret_cast<ReceiverReport*>(rtcp_header);
            // We don't care about the sender of RR but the source of report blocks,
            // since the only valid informations in RR is report blocks.
            // uint32_t sender_ssrc = ntohl(rtcp_sr->packet_sender_ssrc);
            // ssrcs.insert(sender_ssrc);
            for (uint8_t i = 0; i < rtcp_sr->report_count(); i++) {
                uint32_t media_source_ssrc = ntohl(rtcp_sr->GetReportBlock(i)->source_ssrc);
                ssrcs.insert(media_source_ssrc);
                PLOG_VERBOSE << "RTCP RR report block source ssrc= " << media_source_ssrc;
            }
        } 
        // RTP feedback packet (pt = 205) or RTCP payload-specific packet (pt = 206) 
        else if (rtcp_header->payload_type == 205 || rtcp_header->payload_type == 206) {
            auto rtp_fb = reinterpret_cast<RtpFeedback*>(rtcp_header);
            uint32_t sender_ssrc = ntohl(rtp_fb->packet_sender_ssrc);
            ssrcs.insert(sender_ssrc);
            // NOTE: Zero indicates |media_source_ssrc| is useless, like REMB packet.
            uint32_t source_ssrc = ntohl(rtp_fb->media_source_ssrc);
            if (source_ssrc > 0) {
                ssrcs.insert(source_ssrc);
            } else {
                deliver_to_all = true;
                break;
            }
        }
        // RTCP bye (pt = 203)
        else if (rtcp_header->payload_type == 203) {
            auto rtcp_bye = reinterpret_cast<Bye*>(rtcp_header);
            uint32_t sender_ssrc = ntohl(rtcp_bye->packet_sender_ssrc);
            //  Zero indicates the Bye packet is valid but useless, ignoring.
            if (sender_ssrc > 0) {
                ssrcs.insert(sender_ssrc);
            }
        } 
        // RTCP extended report (pt = 207)
        else if (rtcp_header->payload_type == 207) {
            // auto rtcp_xr = reinterpret_cast<ExtendedReport*>(rtcp_header);
            // uint32_t sender_ssrc = ntohl(rtcp_xr->packet_sender_ssrc);
            // ssrcs.insert(sender_ssrc);
            
            // The XR packet is always sent by a receive-only peer,
            // which means |sender_ssrc| is useless for delivering. 
            deliver_to_all = true;
            break;
        } 
        else {
            // TODO: Support more RTCP packet
            PLOG_WARNING << "Unsupport RTCP packet, paylaod type=" << rtcp_header->payload_type;
            deliver_to_all = true;
            break;
        }
    }

    if (deliver_to_all) {
        for (auto& [ssrc, sink] : rtcp_sink_by_ssrc_) {
            sink->OnRtcpPacket(std::move(in_packet));
        }
    } else {
        for (uint32_t ssrc : ssrcs) {
            auto it = rtcp_sink_by_ssrc_.find(ssrc);
            if (it != rtcp_sink_by_ssrc_.end()) {
                it->second->OnRtcpPacket(std::move(in_packet));
            } else {
                PLOG_WARNING << "No sink found for ssrc= " << ssrc;
            }
        }
    }
    return true;
}

    
} // namespace naivertc
