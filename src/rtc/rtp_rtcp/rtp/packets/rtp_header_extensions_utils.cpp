#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

namespace naivertc {
namespace rtp {

bool IsNonVolatile(RtpExtensionType type) {
  switch (type) {
    case kRtpExtensionTransmissionTimeOffset:
    // case kRtpExtensionAudioLevel:
    // case kRtpExtensionCsrcAudioLevel:
    case kRtpExtensionAbsoluteSendTime:
    case kRtpExtensionTransportSequenceNumber:
    case kRtpExtensionTransportSequenceNumber02:
    case kRtpExtensionRtpStreamId:
    case kRtpExtensionMid:
    // case kRtpExtensionGenericFrameDescriptor00:
    // case kRtpExtensionGenericFrameDescriptor02:
      return true;
    // case kRtpExtensionInbandComfortNoise:
    case kRtpExtensionAbsoluteCaptureTime:
    // case kRtpExtensionVideoRotation:
    case kRtpExtensionPlayoutDelay: // FIXME: Why?
    // case kRtpExtensionVideoContentType:
    // case kRtpExtensionVideoLayersAllocation:
    // case kRtpExtensionVideoTiming:
    case kRtpExtensionRepairedRtpStreamId: // FIXME: Why?
    // case kRtpExtensionColorSpace:
    // case kRtpExtensionVideoFrameTrackingId:
      return false;
    case kRtpExtensionNone:
    case kRtpExtensionNumberOfExtensions:
      RTC_NOTREACHED();
      return false;
  }
}
    
} // namespace rtp    
} // namespace naivertc
