#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"
#include "rtc/rtp_rtcp/rtcp_responser.hpp"
#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_controller.hpp"
#include "rtc/media/video/encoded_frame.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/transports/rtc_transport_media.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <vector>

namespace naivertc {

// RtpVideoSender
class RtpVideoSender {
public:
    struct Configuration {
        Clock* clock = nullptr;
        RtcMediaTransport* send_transport = nullptr;

        RtpParameters rtp;
        RtpSenderObservers observers;
    };
public:
    RtpVideoSender(const Configuration& config);
    ~RtpVideoSender();

    bool OnEncodedFrame(video::EncodedFrame encoded_frame);

    void OnRtcpPacket(CopyOnWriteBuffer in_packet);

private:
    void CreateAndInitRtpRtcpModules(const Configuration& config);

    void InitRtpRtcpModules(const RtpParameters& rtp_params);

    std::unique_ptr<FecGenerator> MaybeCreateFecGenerator(const RtpParameters& rtp_params);

private:
    SequenceChecker sequence_checker_;
    const int media_payload_type_;

    std::unique_ptr<RtcpResponser> rtcp_responser_ = nullptr;
    std::unique_ptr<RtpSender> rtp_sender_ = nullptr;
    std::unique_ptr<RtpSenderVideo> sender_video_ = nullptr;
    std::unique_ptr<FecGenerator> fec_generator_ = nullptr;
    // TODO: Implement FecController.
    std::unique_ptr<FecController> fec_controller_ = nullptr;
};

} // namespace naivertc


#endif