#ifndef _RTC_CALL_RTX_REVEIVE_STREAM_H_
#define _RTC_CALL_RTX_REVEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/api/rtp_packet_sink.hpp"

#include <map>
#include <functional>

namespace naivertc {

// This class is responsible for RTX decapsulation.
// The resulting media packets are passed on to a sink
// representing the associated media stream.
class RTC_CPP_EXPORT RtxReceiveStream : public RtpPacketSink {
public:
    RtxReceiveStream(uint32_t media_ssrc,
                     std::map<int, int> associated_payload_types,
                     RtpPacketSink* media_packet_sink);
    ~RtxReceiveStream();

    void OnRtpPacket(RtpPacketReceived rtx_packet) override;

private:
    SequenceChecker sequence_checker_;
    const uint32_t media_ssrc_;
    const std::map<int, int> associated_payload_types_;

    RtpPacketSink* const media_packet_sink_ = nullptr;
};

} // namespace naivertc

#endif