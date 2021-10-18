#include "rtc/call/rtp_demuxer.hpp"
#include "rtc/call/rtp_utils.hpp"

#include <plog/Log.h>

#include <set>

namespace naivertc {
namespace rtcp {

// RTCP common header
struct RTC_CPP_EXPORT Header {
    uint8_t first_byte;
    uint8_t payload_type;
    uint16_t payload_size_in_32bit;
};

// RTCP payload-specific feedback packet (payload type = 206)
struct RTC_CPP_EXPORT PSFeedback {
    Header header;
    uint32_t packet_sender_ssrc;
    uint32_t media_source_ssrc;
};

// RTCP rtp feedback packet (payload type = 205)
struct RTC_CPP_EXPORT RtpFeedback {
    Header header;
    uint32_t packet_sender_ssrc;
    uint32_t media_source_ssrc;
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
    Header header;
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
    Header header;
    uint32_t packet_sender_ssrc;

    ReportBlock report_blocks;

    uint8_t report_count() const { return header.first_byte & 0x1F; }
    const ReportBlock* GetReportBlock(uint8_t num) const { 
        return &report_blocks + sizeof(ReportBlock) * num; 
    }
};
    
} // namespace rtcp

bool RtpDemuxer::DeliverRtcpPacket(CopyOnWriteBuffer in_packet) const {
    if (!IsRtcpPacket(in_packet)) {
        return false;
    }
    std::set<uint32_t> ssrcs;
    size_t packet_size = in_packet.size();
    size_t offset = 0;
    // Parse buffer as a compound packet
    while((sizeof(rtcp::Header) + offset) <= packet_size) {
        auto rtcp_header = reinterpret_cast<rtcp::Header *>(in_packet.data() + offset);
        // Calculate packet size in bytes
        size_t rtcp_packet_size = 4 /* Fixed RTCP header size */ + rtcp_header->payload_size_in_32bit * 4;
        if (rtcp_packet_size > packet_size - offset) {
            break;
        }
        offset += rtcp_packet_size;
        // RTP feedback packet (pt = 205)
        if (rtcp_header->payload_type == 205) {
            auto rtp_fb = reinterpret_cast<rtcp::RtpFeedback*>(rtcp_header);
            ssrcs.insert(rtp_fb->packet_sender_ssrc);
            ssrcs.insert(rtp_fb->media_source_ssrc);
        // RTCP payload-specific packet (pt = 206) 
        } else if (rtcp_header->payload_type == 206) {
            auto ps_fb = reinterpret_cast<rtcp::PSFeedback*>(rtcp_header);
            ssrcs.insert(ps_fb->packet_sender_ssrc);
            ssrcs.insert(ps_fb->media_source_ssrc);
        // RTCP sender report (pt = 200)
        } else if (rtcp_header->payload_type == 200) {
            auto rtcp_sr = reinterpret_cast<rtcp::SenderReport*>(rtcp_header);
            ssrcs.insert(rtcp_sr->packet_sender_ssrc);
            for (uint8_t i = 0; i < rtcp_sr->report_count(); i++) {
                ssrcs.insert(rtcp_sr->GetReportBlock(i)->source_ssrc);
            }
         // RTCP receiver report (pt = 200)
        } else if (rtcp_header->payload_type == 201) {
            auto rtcp_rr = reinterpret_cast<rtcp::ReceiverReport*>(rtcp_header);
            ssrcs.insert(rtcp_rr->packet_sender_ssrc);
            for (uint8_t i = 0; i < rtcp_rr->report_count(); i++) {
                ssrcs.insert(rtcp_rr->GetReportBlock(i)->source_ssrc);
            }
        } else {
            // TODO: Support more RTCP packet
            PLOG_WARNING << "Unsupport RTCP packet, paylaod type=" << rtcp_header->payload_type;
        }
    }
    
    for (uint32_t ssrc : ssrcs) {
        if (auto sink = sink_by_ssrc_.at(ssrc).lock()) {
            sink->OnRtcpPacket(in_packet);
        }
    }

    return true;
}

    
} // namespace naivertc
