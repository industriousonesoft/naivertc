#include "rtc/call/rtp_demuxer.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpDemuxer::RtpDemuxer() {}
RtpDemuxer::~RtpDemuxer() {}

void RtpDemuxer::AddSink(uint32_t ssrc, std::weak_ptr<RtpPacketSink> sink) {
    auto result = sink_by_ssrc_.emplace(ssrc, sink);
    auto it = result.first;
    bool inserted = result.second;
    if (inserted) {
        PLOG_INFO << "Added sink=" << sink.lock()
                  << " bunding with SSRC=" << ssrc;
    } else if (it->second.lock() != sink.lock()) {
        PLOG_INFO << "Update sink=" << sink.lock()
                  << " binding with SSRC=" << ssrc;
    }
}

void RtpDemuxer::RemoveSink(uint32_t ssrc) {
    sink_by_ssrc_.erase(ssrc);
}

void RtpDemuxer::AddSink(std::string mid, std::weak_ptr<RtpPacketSink> sink) {
    auto result = sink_by_mid_.emplace(mid, sink);
    auto it = result.first;
    bool inserted = result.second;
    if (inserted) {
        PLOG_INFO << "Added sink=" << sink.lock()
                  << " bunding with mid=" << mid;
    } else if (it->second.lock() != sink.lock()) {
        PLOG_INFO << "Update sink=" << sink.lock()
                  << " binding with mid=" << mid;
    }
}

void RtpDemuxer::RemoveSink(std::string mid) {
    sink_by_mid_.erase(mid);
}

void RtpDemuxer::OnRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp) {
    if (is_rtcp) {
        DeliverRtcpPacket(std::move(in_packet));
    } else {
        DeliverRtpPacket(std::move(in_packet));
    }
}

} // namespace naivertc