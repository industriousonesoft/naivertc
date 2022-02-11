#ifndef _RTC_CALL_RTP_DEMUXER_H_
#define _RTC_CALL_RTP_DEMUXER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_packet_sink.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

#include <unordered_map>
#include <map>

namespace naivertc {
// This class is not thread-safe, the caller MUST provide that.
class RTC_CPP_EXPORT RtpDemuxer {
public:
    RtpDemuxer();
    ~RtpDemuxer();

    void AddRtcpSink(uint32_t ssrc, RtcpPacketSink* sink);
    void RemoveRtcpSink(uint32_t ssrc);

    void AddRtpSink(uint32_t ssrc, RtpPacketSink* sink);
    void RemoveRtpSink(uint32_t ssrc);

    void AddRtpSink(std::string mid, RtpPacketSink* sink);
    void RemoveRtpSink(std::string mid);

    bool DeliverRtcpPacket(CopyOnWriteBuffer in_packet) const;
    bool DeliverRtpPacket(RtpPacketReceived in_packet) const;

    void Clear();

private:
    
private:
    std::unordered_map<uint32_t, RtpPacketSink*> rtp_sink_by_ssrc_;
    std::unordered_map<uint32_t, RtcpPacketSink*> rtcp_sink_by_ssrc_;
    // FIXME: Why does std::unordered_map not work with std::string key
    std::map<std::string, RtpPacketSink*> rtp_sink_by_mid_;
};

} // namespace naivertc

#endif