#include "rtc/rtp_rtcp/components/rtp_demuxer.hpp"
#include "rtc/rtp_rtcp/base/rtp_utils.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

#include <plog/Log.h>

namespace naivertc {

bool RtpDemuxer::DeliverRtpPacket(CopyOnWriteBuffer in_packet) const {
    if (!IsRtpPacket(in_packet)) {
        return false;
    }

    // TODO: Using RTP header extension map and arrival time as initial parameters
    RtpPacketReceived received_packet;
    if (received_packet.Parse(std::move(in_packet))) {
        PLOG_WARNING << "Failed to parse the incoming RTP packet before demuxing. Drop it.";
        return false;
    }


    // Deliver rtp packet
    if (auto sink = rtp_sink_by_ssrc_.at(received_packet.ssrc())) {
        sink->OnRtpPacket(std::move(received_packet));
    } else  {
        // TODO: Deliver RTP packet by RRID or RSID
        // MID
        auto rtp_mid = received_packet.GetExtension<rtp::RtpMid>();
        // RRID
        auto rtp_rrid = received_packet.GetExtension<rtp::RepairedRtpStreamId>();
        // RSID (rtp stream id) and RRID (repaired rtp stream id) are routed to the same sink.
        // If an RSID is specified on a repaired packet, it should be ignored and the RRID should
        // be used.
        if (!rtp_rrid) {
            rtp_rrid = received_packet.GetExtension<rtp::RtpStreamId>();
        }
        if (rtp_mid) {
            if (auto sink = rtp_sink_by_mid_.at(*rtp_mid)) {
                sink->OnRtpPacket(std::move(received_packet));
            } else {
                PLOG_WARNING << "No sink found for RTP packet, ssrc=" << received_packet.ssrc();
            }
        }
    }

    return true;
}
    
} // namespace naivertc
