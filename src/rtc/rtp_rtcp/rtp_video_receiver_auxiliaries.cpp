#include "rtc/rtp_rtcp/rtp_video_receiver.hpp"

#include <plog/Log.h>

namespace naivertc {

// RtcpFeedbackBuffer
RtpVideoReceiver::RtcpFeedbackBuffer::RtcpFeedbackBuffer(RtcpResponser* sender, 
                                                         KeyFrameRequestSender* key_frame_request_sender) 
    : sender_(sender),
      key_frame_request_sender_(key_frame_request_sender),
      request_key_frame_(false) {}

RtpVideoReceiver::RtcpFeedbackBuffer::~RtcpFeedbackBuffer() = default;

void RtpVideoReceiver::RtcpFeedbackBuffer::SendNack(const std::vector<uint16_t>& nack_list,
                                                    bool buffering_allowed) {
    if (nack_list.empty()) {
        return;
    }
    buffered_nack_list_.insert(buffered_nack_list_.end(), nack_list.cbegin(), nack_list.cend());
    if (!buffering_allowed) {
        SendBufferedRtcpFeedbacks();
    }
}

void RtpVideoReceiver::RtcpFeedbackBuffer::RequestKeyFrame() {
    request_key_frame_ = true;
}

void RtpVideoReceiver::RtcpFeedbackBuffer::SendBufferedRtcpFeedbacks() {
    bool request_key_frame = false;
    std::vector<uint16_t> buffered_nack_list;

    std::swap(request_key_frame_, request_key_frame);
    std::swap(buffered_nack_list_, buffered_nack_list);

    if (request_key_frame) {
        if (key_frame_request_sender_) {
            key_frame_request_sender_->RequestKeyFrame();
        } else {
            PLOG_WARNING << "No key frame request sender available.";
        }
    } else if (!buffered_nack_list.empty()) {
        if (sender_) {
            sender_->SendNack(std::move(buffered_nack_list));
        } else {
            PLOG_WARNING << "No NACK sender available.";
        }
    }
    
}
    
} // namespace naivertc
