#ifndef _RTC_RTP_RTCP_RTP_HEADER_EXTENSIONS_H_
#define _RTC_RTP_RTCP_RTP_HEADER_EXTENSIONS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <vector>
#include <string>

namespace naivertc {

// AbsoluteSendTime
class RTC_CPP_EXPORT AbsoluteSendTimeExtension {
public:
    using value_type = uint32_t;
    static constexpr RtpExtensionType kType = RtpExtensionType::ABSOLUTE_SEND_TIME;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr const char kUri[] = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

    static bool Parse(std::vector<const uint8_t> data, uint32_t* time_24bits);
    static size_t ValueSize(uint32_t time_24bits) { return kValueSizeBytes; }
    static bool Write(std::vector<uint8_t> data, uint32_t time_24bits);

    static constexpr uint32_t MsTo24Bits(int64_t time_ms) {
        return static_cast<uint32_t>(((time_ms << 18) + 500) / 1000) & 0x00FFFFFF;
    }
};

// AbsoluteCaptureTime
class AbsoluteCaptureTimeExtension {
public:
    using value_type = AbsoluteCaptureTime;
    static constexpr RtpExtensionType kType = RtpExtensionType::ABSOLUTE_CAPTURE_TIME;
    static constexpr uint8_t kValueSizeBytes = 16;
    static constexpr uint8_t kValueSizeBytesWithoutEstimatedCaptureClockOffset = 8;
    static constexpr const char kUri[] = "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time";

    static bool Parse(std::vector<const uint8_t> data, AbsoluteCaptureTime* extension);
    static size_t ValueSize(const AbsoluteCaptureTime& extension);
    static bool Write(std::vector<uint8_t> data, const AbsoluteCaptureTime& extension);
};

// TransmissionOffset
class TransmissionOffsetExtension {
public:
    using value_type = int32_t;
    static constexpr RtpExtensionType kType = RtpExtensionType::TRANSMISSTION_TIME_OFFSET;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr const char kUri[] = "urn:ietf:params:rtp-hdrext:toffset";

    static bool Parse(std::vector<const uint8_t> data, int32_t* rtp_time_24bits);
    static size_t ValueSize(int32_t rtp_time_24bits) { return kValueSizeBytes; }
    static bool Write(std::vector<uint8_t> data, int32_t rtp_time_24bits);
};

// TransportSequenceNumber
class TransportSequenceNumberExtension {
public:
    using value_type = uint16_t;
    static constexpr RtpExtensionType kType = RtpExtensionType::TRANSPORT_SEQUENCE_NUMBER;
    static constexpr uint8_t kValueSizeBytes = 2;
    static constexpr const char kUri[] = "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";

    static bool Parse(std::vector<const uint8_t> data, uint16_t* transport_sequence_number);
    static size_t ValueSize(uint16_t /*transport_sequence_number*/) { return kValueSizeBytes; }
    static bool Write(std::vector<uint8_t> data, uint16_t transport_sequence_number);
};

// PlayoutDelayLimits
class PlayoutDelayLimitsExtension {
public:
    using value_type = VideoPlayoutDelay;
    static constexpr RtpExtensionType kType = RtpExtensionType::PLAYOUT_DELAY;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr const char kUri[] = "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

    // Playout delay in milliseconds. A playout delay limit (min or max)
    // has 12 bits allocated. This allows a range of 0-4095 values which
    // translates to a range of 0-40950 in milliseconds.
    static constexpr int kGranularityMs = 10;
    // Maximum playout delay value in milliseconds.
    static constexpr int kMaxMs = 0xfff * kGranularityMs;  // 40950.

    static bool Parse(std::vector<const uint8_t> data, VideoPlayoutDelay* playout_delay);
    static size_t ValueSize(const VideoPlayoutDelay&) { return kValueSizeBytes; }
    static bool Write(std::vector<uint8_t> data, const VideoPlayoutDelay& playout_delay);
};

// Base extension class for RTP header extensions which are strings.
// Subclasses must defined kId and kUri static constexpr members.
class BaseRtpStringExtension {
public:
    using value_type = std::string;
    // String RTP header extensions are limited to 16 bytes because it is the
    // maximum length that can be encoded with one-byte header extensions.
    static constexpr uint8_t kMaxValueSizeBytes = 16;

    static bool Parse(std::vector<const uint8_t> data, std::string* str);
    static size_t ValueSize(const std::string& str) { return str.size(); }
    static bool Write(std::vector<uint8_t> data, const std::string& str);
};

class RtpMidExtension : public BaseRtpStringExtension {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::MID;
    static constexpr const char kUri[] = "urn:ietf:params:rtp-hdrext:sdes:mid";
};
    
} // namespace naivertc


#endif