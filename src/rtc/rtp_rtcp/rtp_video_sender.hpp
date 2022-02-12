#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"
#include "rtc/rtp_rtcp/rtcp_responser.hpp"
#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/media/video/encoded_frame.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/transports/rtc_transport_media.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_controller.hpp"

#include <vector>

namespace naivertc {

// RtpVideoSender
class RTC_CPP_EXPORT RtpVideoSender {
public:
    using Configuration = RtpParameters;
public:
    RtpVideoSender(const Configuration& config,
                   Clock* clock,
                   RtcMediaTransport* send_transport);
    ~RtpVideoSender();

    bool OnEncodedFrame(video::EncodedFrame encoded_frame);

    void OnRtcpPacket(CopyOnWriteBuffer in_packet);

private:
    void CreateAndInitRtpRtcpModules(const Configuration& config,
                                     Clock* clock,
                                     RtcMediaTransport* send_transport);

    void InitRtpRtcpModules(const Configuration& config);

    std::unique_ptr<FecGenerator> MaybeCreateFecGenerator(const Configuration& config, uint32_t media_ssrc);

private:
    SequenceChecker sequence_checker_;
    const int media_payload_type_;

    std::unique_ptr<RtcpResponser> rtcp_responser_ = nullptr;
    std::unique_ptr<RtpSender> rtp_sender_ = nullptr;
    std::unique_ptr<RtpSenderVideo> sender_video_ = nullptr;
    std::unique_ptr<FecGenerator> fec_generator_ = nullptr;
    std::unique_ptr<FecGenerator> fec_controller_ = nullptr;
};

} // namespace naivertc


#endif