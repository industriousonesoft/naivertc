#ifndef _RTC_RTP_RTCP_RTP_HEADER_EXTENSIONS_H_
#define _RTC_RTP_RTCP_RTP_HEADER_EXTENSIONS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <vector>
#include <string>

namespace naivertc {
namespace rtp {
namespace extension {

// AbsoluteSendTime
class RTC_CPP_EXPORT AbsoluteSendTime {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::ABSOLUTE_SEND_TIME;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr const char kUri[] = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

    static constexpr uint32_t MsTo24Bits(int64_t time_ms) {
        return static_cast<uint32_t>(((time_ms << 18) + 500) / 1000) & 0x00FFFFFF;
    }
public:
    AbsoluteSendTime(uint32_t* time_24bits);
    ~AbsoluteSendTime();
    
    size_t data_size() const { return kValueSizeBytes; }
    uint32_t time_24bits() const { return time_24bits_; }

    bool Parse(std::vector<const uint8_t> data);
    bool PackInto(std::vector<uint8_t> data) const;
private:
    uint32_t time_24bits_;
};

// AbsoluteCaptureTime
// The Absolute Capture Time extension is used to stamp RTP packets with a NTP
// timestamp showing when the first audio or video frame in a packet was
// originally captured. The intent of this extension is to provide a way to
// accomplish audio-to-video synchronization when RTCP-terminating intermediate
// systems (e.g. mixers) are involved. See:
// http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time
class AbsoluteCaptureTime {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::ABSOLUTE_CAPTURE_TIME;
    static constexpr uint8_t kValueSizeBytes = 16;
    static constexpr uint8_t kValueSizeBytesWithoutEstimatedCaptureClockOffset = 8;
    static constexpr const char kUri[] = "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time";

public:
    AbsoluteCaptureTime(uint64_t absolute_capture_timestamp, std::optional<int64_t> estimated_capture_clock_offset);
    ~AbsoluteCaptureTime();

    size_t data_size() const;
    uint64_t absolute_capture_timestamp() const { return absolute_capture_timestamp_; }
    std::optional<int64_t> estimated_capture_clock_offset() const { return estimated_capture_clock_offset_; }

    bool Parse(std::vector<const uint8_t> data);
    bool PackInto(std::vector<uint8_t> data) const;
private:
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
    uint64_t absolute_capture_timestamp_;

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
    std::optional<int64_t> estimated_capture_clock_offset_;
};

// TransmissionOffset
class TransmissionOffset {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::TRANSMISSTION_TIME_OFFSET;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr const char kUri[] = "urn:ietf:params:rtp-hdrext:toffset";

public:
    TransmissionOffset(int32_t rtp_time_24bits);
    ~TransmissionOffset();

    size_t data_size() const { return kValueSizeBytes; }
    int32_t rtp_time_24bits() const { return rtp_time_24bits_; }

    bool Parse(std::vector<const uint8_t> data);
    bool PackInto(std::vector<uint8_t> data) const;
private:
    int32_t rtp_time_24bits_;
};

// TransportSequenceNumber
class TransportSequenceNumber {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::TRANSPORT_SEQUENCE_NUMBER;
    static constexpr uint8_t kValueSizeBytes = 2;
    static constexpr const char kUri[] = "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";

public:
    TransportSequenceNumber(uint16_t transport_sequence_number);
    ~TransportSequenceNumber();

    size_t data_size() const { return kValueSizeBytes; }  
    uint16_t transport_sequence_number() const { return transport_sequence_number_; }

    bool Parse(std::vector<const uint8_t> data);
    bool PackInto(std::vector<uint8_t> data) const;
private:
    uint16_t transport_sequence_number_;
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
class PlayoutDelayLimits {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::PLAYOUT_DELAY;
    static constexpr uint8_t kValueSizeBytes = 3;
    static constexpr const char kUri[] = "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

    // Playout delay in milliseconds. A playout delay limit (min or max)
    // has 12 bits allocated. This allows a range of 0-4095 values which
    // translates to a range of 0-40950 in milliseconds.
    static constexpr int kGranularityMs = 10;
    // Maximum playout delay value in milliseconds.
    static constexpr int kMaxMs = 0xfff * kGranularityMs;  // 40950.

public:
    PlayoutDelayLimits();
    PlayoutDelayLimits(int min_ms, int max_ms);
    ~PlayoutDelayLimits();
    
    size_t data_size() const { return kValueSizeBytes; }
    int min_ms() const { return min_ms_; }
    int max_ms() const { return max_ms_; }

    bool Parse(std::vector<const uint8_t> data);
    bool PackInto(std::vector<uint8_t> data) const;

    bool operator==(const PlayoutDelayLimits& rhs) const {
        return min_ms_ == rhs.min_ms_ && max_ms_ == rhs.max_ms_;
    }

private:
    int min_ms_ = -1;
    int max_ms_ = -1;
};

// Base extension class for RTP header extensions which are strings.
// Subclasses must defined kId and kUri static constexpr members.
class BaseRtpString {
public:
    // String RTP header extensions are limited to 16 bytes because it is the
    // maximum length that can be encoded with one-byte header extensions.
    static constexpr uint8_t kMaxValueSizeBytes = 16;
public:
    BaseRtpString(const std::string str);
    ~BaseRtpString();

    size_t data_size() const { return value_.size(); }
    std::string_view value() const { return value_; }

    bool Parse(std::vector<const uint8_t> data);
    bool PackInto(std::vector<uint8_t> data) const;
private:
    std::string value_;
};

class RtpMid : public BaseRtpString {
public:
    static constexpr RtpExtensionType kType = RtpExtensionType::MID;
    static constexpr const char kUri[] = "urn:ietf:params:rtp-hdrext:sdes:mid";
public:
    RtpMid(const std::string value);
    ~RtpMid();
};
    
} // namespace extension
} // namespace rtp
} // namespace naivertc


#endif