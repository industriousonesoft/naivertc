#ifndef _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_
#define _RTC_CALL_RTP_VIDEO_STREAM_RECEIVER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"

#include <memory>

namespace naivertc {

class RtpPacketReceived;
class CopyOnWriteBuffer;

class RTC_CPP_EXPORT RtpVideoStreamReceiver {
public:
    RtpVideoStreamReceiver(std::shared_ptr<TaskQueue> task_queue);
    ~RtpVideoStreamReceiver();

    void OnIncomingRtcpPacket(CopyOnWriteBuffer in_packet);
    void OnIncomingRtpPacket(RtpPacketReceived in_packet);

private:
    void OnEmptyPacketReceived(uint16_t seq_num);

private:
    std::shared_ptr<TaskQueue> task_queue_;
    
};
    
} // namespace naivertc


#endif