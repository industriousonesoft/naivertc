#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

namespace naivertc {
namespace rtp {

bool IsNonVolatile(RtpExtensionType type) {
  switch (type) {
    case RtpExtensionType::TRANSMISSTION_TIME_OFFSET:
    // case kRtpExtensionAudioLevel:
    // case kRtpExtensionCsrcAudioLevel:
    case RtpExtensionType::ABSOLUTE_SEND_TIME:
    case RtpExtensionType::TRANSPORT_SEQUENCE_NUMBER:
    // case kRtpExtensionTransportSequenceNumber02:
    case RtpExtensionType::RTP_STREAM_ID:
    case RtpExtensionType::MID:
    // case kRtpExtensionGenericFrameDescriptor00:
    // case kRtpExtensionGenericFrameDescriptor02:
      return true;
    // case kRtpExtensionInbandComfortNoise:
    case RtpExtensionType::ABSOLUTE_CAPTURE_TIME:
    // case kRtpExtensionVideoRotation:
    case RtpExtensionType::PLAYOUT_DELAY_LIMITS: // FIXME: Why?
    // case kRtpExtensionVideoContentType:
    // case kRtpExtensionVideoLayersAllocation:
    // case kRtpExtensionVideoTiming:
    case RtpExtensionType::REPAIRED_RTP_STREAM_ID: // FIXME: Why?
    // case kRtpExtensionColorSpace:
    // case kRtpExtensionVideoFrameTrackingId:
      return false;
    case RtpExtensionType::NONE:
    case RtpExtensionType::NUMBER_OF_EXTENSIONS:
      RTC_NOTREACHED();
      return false;
  }
}
    
} // namespace rtp    
} // namespace naivertc
