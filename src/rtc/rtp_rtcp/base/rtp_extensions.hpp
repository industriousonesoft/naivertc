#ifndef _RTC_RTP_RTCP_BASE_RTP_EXTENSIONS_H_
#define _RTC_RTP_RTCP_BASE_RTP_EXTENSIONS_H_

#include "base/defines.hpp"

#include <string>

namespace naivertc {

// Rtp header extensions type
enum RtpExtensionType : int {
    kRtpExtensionNone = 0,
    kRtpExtensionTransmissionTimeOffset,
    // kRtpExtensionAudioLevel,
    // kRtpExtensionCsrcAudioLevel,
    // kRtpExtensionInbandComfortNoise,
    kRtpExtensionAbsoluteSendTime,
    kRtpExtensionAbsoluteCaptureTime,
    // kRtpExtensionVideoRotation,
    kRtpExtensionTransportSequenceNumber,
    // kRtpExtensionTransportSequenceNumber02,
    kRtpExtensionPlayoutDelay,
    // kRtpExtensionVideoContentType,
    // kRtpExtensionVideoLayersAllocation,
    // kRtpExtensionVideoTiming,
    kRtpExtensionRtpStreamId,
    kRtpExtensionRepairedRtpStreamId,
    kRtpExtensionMid,
    // kRtpExtensionGenericFrameDescriptor00,
    // kRtpExtensionGenericFrameDescriptor = kRtpExtensionGenericFrameDescriptor00,
    // kRtpExtensionGenericFrameDescriptor02,
    // kRtpExtensionColorSpace,
    // kRtpExtensionVideoFrameTrackingId,
    kRtpExtensionNumberOfExtensions  // Must be the last entity in the enum.
};

// RtpExtension
struct RTC_CPP_EXPORT RtpExtension {

    static constexpr int kInvalidId = 0;
    static constexpr int kMinId = 1;
    static constexpr int kMaxId = 255;
    static constexpr int kMaxValueSize = 255;
    static constexpr int kOneByteHeaderExtensionMaxId = 14;
    static constexpr int kOneByteHeaderExtensionReservedId = 15; // The maximum value of 4 bits
    static constexpr int kOneByteHeaderExtensionMaxValueSize = 16; // The maximum value of 4 bits + 1

    // Header extension for RTP timestamp offset, see RFC 5450 for details:
    // http://tools.ietf.org/html/rfc5450
    static constexpr char kTimestampOffsetUri[] =
        "urn:ietf:params:rtp-hdrext:toffset";

    // Header extension for absolute send time, see url for details:
    // http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
    static constexpr char kAbsSendTimeUri[] =
        "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

    // Header extension for absolute capture time, see url for details:
    // http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time
    static constexpr char kAbsoluteCaptureTimeUri[] =
        "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time";

    // Header extension for transport sequence number, see url for details:
    // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions
    static constexpr char kTransportSequenceNumberUri[] =
        "http://www.ietf.org/id/"
        "draft-holmer-rmcat-transport-wide-cc-extensions-01";

    // This extension allows applications to adaptively limit the playout delay
    // on frames as per the current needs. For example, a gaming application
    // has very different needs on end-to-end delay compared to a video-conference
    // application.
    static constexpr char kPlayoutDelayUri[] =
        "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

    // Header extension for identifying media section within a transport.
    // https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-49#section-15
    static constexpr char kMidUri[] = "urn:ietf:params:rtp-hdrext:sdes:mid";

    // Header extension for RIDs and Repaired RIDs
    // https://tools.ietf.org/html/draft-ietf-avtext-rid-09
    // https://tools.ietf.org/html/draft-ietf-mmusic-rid-15
    static constexpr char kRidUri[] =
        "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
    static constexpr char kRepairedRidUri[] =
        "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";

    int id = kInvalidId;
    std::string uri;

    RtpExtension(int id, std::string uri) 
        : id(id),
          uri(uri) {}
};


} // namespace naivertc

#endif