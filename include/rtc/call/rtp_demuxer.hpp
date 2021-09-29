#ifndef _RTC_CALL_RTP_DEMUXER_H_
#define _RTC_CALL_RTP_DEMUXER_H_

#include "base/defines.hpp"
#include "rtc/call/rtp_packet_sink.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

#include <unordered_map>
#include <map>

namespace naivertc {
// This class is not thread-safe, the caller MUST provide that.
class RTC_CPP_EXPORT RtpDemuxer {
public:
    RtpDemuxer();
    ~RtpDemuxer();

    void AddSink(uint32_t ssrc, std::weak_ptr<RtpPacketSink> sink);
    void RemoveSink(uint32_t ssrc);

    void AddSink(std::string mid, std::weak_ptr<RtpPacketSink> sink);
    void RemoveSink(std::string mid);

    void OnRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp);

private:
    std::unordered_map<uint32_t, std::weak_ptr<RtpPacketSink>> sink_by_ssrc_;
    // FIXME: Why does std::unordered_map not work with std::string key
    std::map<std::string, std::weak_ptr<RtpPacketSink>> sink_by_mid_;
};

} // namespace naivertc

#endif