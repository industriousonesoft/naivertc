#include "rtc/rtp_rtcp/components/rtp_demuxer.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpDemuxer::RtpDemuxer() {}
RtpDemuxer::~RtpDemuxer() {}

void RtpDemuxer::AddRtcpSink(uint32_t ssrc, RtcpPacketSink* sink) {
    auto result = rtcp_sink_by_ssrc_.emplace(ssrc, sink);
    auto it = result.first;
    bool inserted = result.second;
    if (inserted) {
        PLOG_INFO << "Added RTCP sink bunding with SSRC=" << ssrc;
    } else if (it->second != sink) {
        PLOG_INFO << "Update RTCP sink binding with SSRC=" << ssrc;
    }
}

void RtpDemuxer::RemoveRtcpSink(uint32_t ssrc) {
    rtcp_sink_by_ssrc_.erase(ssrc);
}

void RtpDemuxer::AddRtpSink(uint32_t ssrc, RtpPacketSink* sink) {
    auto result = rtp_sink_by_ssrc_.emplace(ssrc, sink);
    auto it = result.first;
    bool inserted = result.second;
    if (inserted) {
        PLOG_INFO << "Added RTP sink bunding with SSRC=" << ssrc;
    } else if (it->second != sink) {
        PLOG_INFO << "Update RTP sink binding with SSRC=" << ssrc;
    }
}

void RtpDemuxer::RemoveRtpSink(uint32_t ssrc) {
    rtp_sink_by_ssrc_.erase(ssrc);
}

void RtpDemuxer::AddRtpSink(std::string mid, RtpPacketSink* sink) {
    auto result = rtp_sink_by_mid_.emplace(mid, sink);
    auto it = result.first;
    bool inserted = result.second;
     if (inserted) {
        PLOG_INFO << "Added RTCP sink bunding with mid=" << mid;
    } else if (it->second != sink) {
        PLOG_INFO << "Update RTCP sink binding with mid=" << mid;
    }
}

void RtpDemuxer::RemoveRtpSink(std::string mid) {
    rtp_sink_by_mid_.erase(mid);
}

void RtpDemuxer::OnRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp) {
    if (is_rtcp) {
        DeliverRtcpPacket(std::move(in_packet));
    } else {
        DeliverRtpPacket(std::move(in_packet));
    }
}

void RtpDemuxer::Clear() {
    rtp_sink_by_mid_.clear();
    rtp_sink_by_ssrc_.clear();
    rtcp_sink_by_ssrc_.clear();
}

} // namespace naivertc