#ifndef _RTC_RTP_RTCP_RTP_HEADER_EXTENSIONS_H_
#define _RTC_RTP_RTCP_RTP_HEADER_EXTENSIONS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_extensions.hpp"
#include "rtc/media/video/common.hpp"
#include "common/array_view.hpp"

#include <vector>
#include <string>
#include <optional>

namespace naivertc {
namespace rtp {

// AbsoluteSendTime
class AbsoluteSendTime final {
public:
    using ValueType = uint32_t;
    static constexpr RtpExtensionType kType = kRtpExtensionAbsoluteSendTime;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr std::string_view kUri = RtpExtension::kAbsSendTimeUri;

    static constexpr uint32_t MsTo24Bits(int64_t time_ms) {
        return static_cast<uint32_t>(((time_ms << 18) + 500) / 1000) & 0x00FFFFFF;
    }

    static bool Parse(ArrayView<const uint8_t> data, uint32_t* time_24bits);
    static bool Write(ArrayView<uint8_t> data, uint32_t time_24bits);
    static size_t ValueSize(uint32_t time_24bits) { return kValueSizeBytes; };
};

// AbsoluteCaptureTime
// The Absolute Capture Time extension is used to stamp RTP packets with a NTP
// timestamp showing when the first audio or video frame in a packet was
// originally captured. The intent of this extension is to provide a way to
// accomplish audio-to-video synchronization when RTCP-terminating intermediate
// systems (e.g. mixers) are involved. See:
// http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time
class AbsoluteCaptureTime final {
public:
    struct TimeInfo {
        // Absolute capture timestamp is the NTP timestamp of when the first frame in
        // a packet was originally captured. This timestamp MUST be based on the same
        // clock as the clock used to generate NTP timestamps for RTCP sender reports
        // on the capture system.
        //
        // It’s not always possible to do an NTP clock readout at the exact moment of
        // when a media frame is captured. A capture system MAY postpone the readout
        // until a more convenient time. A capture system SHOULD have known delays
        // (e.g. from hardware buffers) subtracted from the readout to make the final
        // timestamp as close to the actual capture time as possible.
        //
        // This field is encoded as a 64-bit unsigned fixed-point number with the high
        // 32 bits for the timestamp in seconds and low 32 bits for the fractional
        // part. This is also known as the UQ32.32 format and is what the RTP
        // specification defines as the canonical format to represent NTP timestamps.
        uint64_t absolute_capture_timestamp = 0;

        // Estimated capture clock offset is the sender’s estimate of the offset
        // between its own NTP clock and the capture system’s NTP clock. The sender is
        // here defined as the system that owns the NTP clock used to generate the NTP
        // timestamps for the RTCP sender reports on this stream. The sender system is
        // typically either the capture system or a mixer.
        //
        // This field is encoded as a 64-bit two’s complement signed fixed-point
        // number with the high 32 bits for the seconds and low 32 bits for the
        // fractional part. It’s intended to make it easy for a receiver, that knows
        // how to estimate the sender system’s NTP clock, to also estimate the capture
        // system’s NTP clock:
        //
        // Capture NTP Clock = Sender NTP Clock + Capture Clock Offset
        std::optional<int64_t> estimated_capture_clock_offset;
    };
    
public:
    using ValueType = TimeInfo;
    static constexpr RtpExtensionType kType = kRtpExtensionAbsoluteCaptureTime;
    static constexpr uint8_t kValueSizeBytes = 16;
    static constexpr uint8_t kValueSizeBytesWithoutEstimatedCaptureClockOffset = 8;
    static constexpr std::string_view kUri = RtpExtension::kAbsoluteCaptureTimeUri;

    static bool Parse(ArrayView<const uint8_t> data, TimeInfo* time_info);
    static bool Write(ArrayView<uint8_t> data, const TimeInfo& time_info);
    static size_t ValueSize(const TimeInfo& time_info);
private:

};

// TransmissionTimeOffset
class TransmissionTimeOffset final {
public:
    using ValueType = int32_t;
    static constexpr RtpExtensionType kType = kRtpExtensionTransmissionTimeOffset;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr std::string_view kUri = RtpExtension::kTimestampOffsetUri;

    static bool Parse(ArrayView<const uint8_t> data, int32_t* rtp_time_24bits);
    static bool Write(ArrayView<uint8_t> data, int32_t rtp_time_24bits);
    static size_t ValueSize(int32_t rtp_time_24bits) { return kValueSizeBytes; };
};

// TransportSequenceNumber
class TransportSequenceNumber final {
public:
    using ValueType = uint16_t;
    static constexpr RtpExtensionType kType = kRtpExtensionTransportSequenceNumber;
    static constexpr uint8_t kValueSizeBytes = 2;
    static constexpr std::string_view kUri = RtpExtension::kTransportSequenceNumberUri;

    static bool Parse(ArrayView<const uint8_t> data, uint16_t* transport_sequence_number);
    static bool Write(ArrayView<uint8_t> data, uint16_t transport_sequence_number);
    static size_t ValueSize(uint16_t transport_sequence_number) { return kValueSizeBytes; };
};

class TransportSequenceNumberV2 final {
public:
    struct FeedbackRequest {
        // Determines whether the recv delta as specified in
        // https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
        // should be included.
        bool include_timestamps;
        // Include feedback of received packets in the range [sequence_number -
        // sequence_count + 1, sequence_number]. That is, no feedback will be sent if
        // sequence_count is zero.
        int sequence_count;
    };
public:
    static constexpr RtpExtensionType kType = kRtpExtensionTransportSequenceNumber02;
    static constexpr uint8_t kValueSizeBytes = 4;
    static constexpr uint8_t kValueSizeBytesWithoutFeedbackRequest = 2;
    static constexpr std::string_view kUri = RtpExtension::kTransportSequenceNumberV2Uri;

    static bool Parse(ArrayView<const uint8_t> data,
                      uint16_t* transport_sequence_number,
                      std::optional<FeedbackRequest>* feedback_request);
    static size_t ValueSize( uint16_t /*transport_sequence_number*/,
                            const std::optional<FeedbackRequest>& feedback_request) {
        return feedback_request ? kValueSizeBytes
                                : kValueSizeBytesWithoutFeedbackRequest;
    }
    static bool Write(ArrayView<uint8_t> data,
                         uint16_t transport_sequence_number,
                         const std::optional<FeedbackRequest>& feedback_request);

private:
    static constexpr uint16_t kIncludeTimestampsBit = 1 << 15;
};

// PlayoutDelayLimits
// Minimum and maximum playout delay values from capture to render.
// These are best effort values.
//
// A value < 0 indicates no change from previous valid value.
//
// min = max = 0 indicates that the receiver should try and render
// frame as soon as possible.
//
// min = x, max = y indicates that the receiver is free to adapt
// in the range (x, y) based on network jitter.
class PlayoutDelayLimits final {
public:
    using ValueType = naivertc::video::PlayoutDelay;
    static constexpr RtpExtensionType kType = kRtpExtensionPlayoutDelay;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr std::string_view kUri = RtpExtension::kPlayoutDelayUri;

    // Playout delay in milliseconds. A playout delay limit (min or max)
    // has 12 bits allocated. This allows a range of 0-4095 values which
    // translates to a range of 0-40950 in milliseconds.
    static constexpr int kGranularityMs = 10;
    // Maximum playout delay value in milliseconds.
    static constexpr int kMaxMs = 0xfff * kGranularityMs;  // 40950.

    static bool Parse(ArrayView<const uint8_t> data, ValueType* playout_delay);
    static bool Write(ArrayView<uint8_t> data, const ValueType& playout_delay);
    static size_t ValueSize(const ValueType& playout_delay) { return kValueSizeBytes; };
};

// Base extension class for RTP header extensions which are strings.
// Subclasses must defined kId and kUri static constexpr members.
class BaseRtpString {
public:
    using ValueType = std::string;
    // String RTP header extensions are limited to 16 bytes because it is the
    // maximum length that can be encoded with one-byte header extensions.
    static constexpr uint8_t kMaxValueSizeBytes = 16;
    static bool Parse(ArrayView<const uint8_t> data, std::string* value);
    static bool Write(ArrayView<uint8_t> data, const std::string& value);
    static size_t ValueSize(const std::string value) { return value.size(); };

};

// RtpMid
class RtpMid final : public BaseRtpString {
public:
    static constexpr RtpExtensionType kType = kRtpExtensionMid;
    static constexpr std::string_view kUri = RtpExtension::kMidUri;
};

// RtpStreamId
class RtpStreamId : public BaseRtpString {
public:
    static constexpr RtpExtensionType kType = kRtpExtensionRtpStreamId;
    static constexpr std::string_view kUri = RtpExtension::kRidUri;
};

// RepairedRtpStreamId
class RepairedRtpStreamId : public BaseRtpString {
 public:
  static constexpr RtpExtensionType kType = kRtpExtensionRepairedRtpStreamId;
  static constexpr std::string_view kUri = RtpExtension::kRepairedRidUri;
};

// Utils methods

// Non-volatile extensions can be expected on all packets, if registered.
// Volatile ones, such as VideoContentTypeExtension which is only set on
// key-frames, are removed to simplify overhead calculations at the expense of
// some accuracy.
bool IsNonVolatile(RtpExtensionType type);
    
} // namespace rtp
} // namespace naivertc


#endif