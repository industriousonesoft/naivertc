#include "rtc/call/rtp_video_stream_receiver.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

namespace naivertc {

RtpVideoStreamReceiver::RtpVideoStreamReceiver(std::shared_ptr<TaskQueue> task_queue) 
    : task_queue_(std::move(task_queue)) {}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {}

void RtpVideoStreamReceiver::OnIncomingRtcpPacket(CopyOnWriteBuffer in_packet) {
    // task_queue_->Async([](){

    // });
}

void RtpVideoStreamReceiver::OnIncomingRtpPacket(RtpPacketReceived in_packet) {
    task_queue_->Async([this, in_packet=std::move(in_packet)](){
        // Padding or keep-alive packet
        if (in_packet.payload_size() == 0) {
            OnEmptyPacketReceived(in_packet.sequence_number());
            return;
        }
    });
}

// Private methods
void RtpVideoStreamReceiver::OnEmptyPacketReceived(uint16_t seq_num) {

}

} // namespace naivertc
