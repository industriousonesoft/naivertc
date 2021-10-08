#ifndef _RTC_CALL_RTX_REVEIVE_STREAM_H_
#define _RTC_CALL_RTX_REVEIVE_STREAM_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

#include <map>
#include <functional>

namespace naivertc {

// This class is responsible for RTX decapsulation.
// The resulting media packets are passed on to a sink
// representing the associated media stream.
class RTC_CPP_EXPORT RtxReceiveStream {
public:
    RtxReceiveStream(uint32_t media_ssrc,
                     std::map<int, int> associated_payload_types,
                     std::shared_ptr<TaskQueue> task_queue);
    ~RtxReceiveStream();

    void OnRtxPacket(RtpPacketReceived rtx_packet);

    using MediaPacketRecoveredCallback = std::function<void(RtpPacketReceived media_packet)>;
    void OnMediaPacketRecovered(MediaPacketRecoveredCallback callback);

private:
    const uint32_t media_ssrc_;
    const std::map<int, int> associated_payload_types_;
    std::shared_ptr<TaskQueue> task_queue_;

    MediaPacketRecoveredCallback media_packet_recovered_callback_ = nullptr;
};

} // namespace naivertc

#endif