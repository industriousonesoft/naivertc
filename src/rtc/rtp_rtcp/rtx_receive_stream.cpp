#include "rtc/rtp_rtcp/rtx_receive_stream.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"

#include <plog/Log.h>

namespace naivertc {

RtxReceiveStream::RtxReceiveStream(uint32_t media_ssrc,
                                   std::unordered_map<int, int> associated_payload_types,
                                   RtpPacketSink* media_packet_sink) 
    : media_ssrc_(media_ssrc),
      associated_payload_types_(std::move(associated_payload_types)),
      media_packet_sink_(media_packet_sink) {
    if (associated_payload_types_.empty()) {
        PLOG_WARNING << "RtxReceiveStream created with empty associated payload type mapping.";
    }
}

RtxReceiveStream::~RtxReceiveStream() {}

void RtxReceiveStream::OnRtpPacket(RtpPacketReceived rtx_packet) {
    RTC_RUN_ON(&sequence_checker_);
    auto payload = rtx_packet.payload();
    if (payload.size() < kRtxHeaderSize) {
        return;
    }

    auto it = associated_payload_types_.find(rtx_packet.payload_type());
    if (it == associated_payload_types_.end()) {
        PLOG_VERBOSE << "Unknown payload type "
                     << static_cast<int>(rtx_packet.payload_type())
                     << " on rtx ssrc=" << rtx_packet.ssrc();
        return;
    }
    RtpPacketReceived media_packet;
    media_packet.CopyHeaderFrom(rtx_packet);
    // Set the media ssrc
    media_packet.set_ssrc(media_ssrc_);
    // The media sequence number is saved in first two byte of RTX packet payload 
    media_packet.set_sequence_number((payload[0] << 8) + payload[1]);
    media_packet.set_payload_type(it->second);
    media_packet.set_is_recovered(true);
    media_packet.set_arrival_time(rtx_packet.arrival_time());

    auto rtx_payload = payload.subview(kRtxHeaderSize);
    uint8_t* media_payload = media_packet.AllocatePayload(rtx_payload.size());
    assert(media_payload != nullptr);
    memcpy(media_payload, rtx_payload.data(), rtx_payload.size());

    if (media_packet_sink_) {
        media_packet_sink_->OnRtpPacket((std::move(media_packet)));
    }
}

} // namespace naivertc