#include "rtc/media/video/h264_rtp_packetizer.hpp"

namespace naivertc {

// 参考webrtc: modules/rtp_rtcp/rtp_format_h264.h
H264RtpPacketizer::H264RtpPacketizer(std::shared_ptr<RtpPacketizationConfig> rtp_config, 
                                     PaylaodSizeLimits limits, 
                                     H264::PacketizationMode packetization_mode) 
    : RtpPacketizer(rtp_config, limits),
    packetization_mode_(packetization_mode) {}

H264RtpPacketizer::~H264RtpPacketizer() {}



} // namespace naivertc