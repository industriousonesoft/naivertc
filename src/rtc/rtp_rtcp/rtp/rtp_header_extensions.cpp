#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

namespace naivertc {
namespace rtp {

// Absolute send time in RTP streams.
//
// The absolute send time is signaled to the receiver in-band using the
// general mechanism for RTP header extensions [RFC8285]. The payload
// of this extension (the transmitted value) is a 24-bit unsigned integer
// containing the sender's current time in seconds as a fixed point number
// with 18 bits fractional part.
//
// The form of the absolute send time extension block:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=2 |              absolute send time               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

AbsoluteSendTime::AbsoluteSendTime() : AbsoluteSendTime(0) {}

AbsoluteSendTime::AbsoluteSendTime(uint32_t time_24bits) 
    : time_24bits_(time_24bits) {}

AbsoluteSendTime::~AbsoluteSendTime() {}

bool AbsoluteSendTime::Parse(const uint8_t* data, size_t size) {
    if (size != kValueSizeBytes)
        return false;
    time_24bits_ = ByteReader<uint32_t, 3>::ReadBigEndian(data);
    return true;
}

bool AbsoluteSendTime::PackInto(uint8_t* data, size_t size) const {
    if(size != kValueSizeBytes) return false;
    if(time_24bits_ > 0x00FFFFFF) return false;
    ByteWriter<uint32_t, 3>::WriteBigEndian(data, time_24bits_);
    return true;
}

// Absolute Capture Time
//
// The Absolute Capture Time extension is used to stamp RTP packets with a NTP
// timestamp showing when the first audio or video frame in a packet was
// originally captured. The intent of this extension is to provide a way to
// accomplish audio-to-video synchronization when RTCP-terminating intermediate
// systems (e.g. mixers) are involved.
//
// Data layout of the shortened version of abs-capture-time:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=7 |     absolute capture timestamp (bit 0-23)     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |             absolute capture timestamp (bit 24-55)            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ... (56-63)  |
//   +-+-+-+-+-+-+-+-+
//
// Data layout of the extended version of abs-capture-time:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=15|     absolute capture timestamp (bit 0-23)     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |             absolute capture timestamp (bit 24-55)            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ... (56-63)  |   estimated capture clock offset (bit 0-23)   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |           estimated capture clock offset (bit 24-55)          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ... (56-63)  |
//   +-+-+-+-+-+-+-+-+
AbsoluteCaptureTime::AbsoluteCaptureTime() 
    : AbsoluteCaptureTime(0, std::nullopt) {}

AbsoluteCaptureTime::AbsoluteCaptureTime(uint64_t absolute_capture_timestamp, 
                                         std::optional<int64_t> estimated_capture_clock_offset) 
    : absolute_capture_timestamp_(absolute_capture_timestamp),
      estimated_capture_clock_offset_(estimated_capture_clock_offset) {

}
    
AbsoluteCaptureTime::~AbsoluteCaptureTime() {}

bool AbsoluteCaptureTime::Parse(const uint8_t* data, size_t size) {
    if (size != kValueSizeBytes || /*FIXME: WebRTC use && here? */
        size != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        return false;
    }

    absolute_capture_timestamp_ = ByteReader<uint64_t>::ReadBigEndian(data);

    if (size != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        estimated_capture_clock_offset_ = ByteReader<int64_t>::ReadBigEndian(data + 8);
    }

    return true;
}

size_t AbsoluteCaptureTime::data_size() const {
    if (estimated_capture_clock_offset_ != std::nullopt) {
        return kValueSizeBytes;
    } else {
        return kValueSizeBytesWithoutEstimatedCaptureClockOffset;
    }
}

bool AbsoluteCaptureTime::PackInto(uint8_t* data, size_t size) const {
    if(size != data_size()) return false;

    ByteWriter<uint64_t>::WriteBigEndian(data, absolute_capture_timestamp_);

    if (size != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        ByteWriter<int64_t>::WriteBigEndian(data + 8, estimated_capture_clock_offset_.value());
    }

    return true;
}

// From RFC 5450: Transmission Time Offsets in RTP Streams.
//
// The transmission time is signaled to the receiver in-band using the
// general mechanism for RTP header extensions [RFC8285]. The payload
// of this extension (the transmitted value) is a 24-bit signed integer.
// When added to the RTP timestamp of the packet, it represents the
// "effective" RTP transmission time of the packet, on the RTP
// timescale.
//
// The form of the transmission offset extension block:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=2 |              transmission offset              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TransmissionTimeOffset::TransmissionTimeOffset() 
    : TransmissionTimeOffset(0) {}

TransmissionTimeOffset::TransmissionTimeOffset(int32_t rtp_time_24bits) 
    : rtp_time_24bits_(rtp_time_24bits) {}
    
TransmissionTimeOffset::~TransmissionTimeOffset() {}

bool TransmissionTimeOffset::Parse(const uint8_t* data, size_t size) {
    if (size != kValueSizeBytes) return false;
    rtp_time_24bits_ = ByteReader<int32_t, 3>::ReadBigEndian(data);
    return true;
}

bool TransmissionTimeOffset::PackInto(uint8_t* data, size_t size) const {
    if(size != kValueSizeBytes) return false;
    if(rtp_time_24bits_ > 0x00ffffff) return false;
    ByteWriter<int32_t, 3>::WriteBigEndian(data, rtp_time_24bits_);
    return true;
}

// TransportSequenceNumber
//
//   0                   1                   2
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=1   |transport-wide sequence number |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
TransportSequenceNumber::TransportSequenceNumber() 
    : TransportSequenceNumber(0) {}

TransportSequenceNumber::TransportSequenceNumber(uint16_t transport_sequence_number) 
    : transport_sequence_number_(transport_sequence_number){}

TransportSequenceNumber::~TransportSequenceNumber() {}

bool TransportSequenceNumber::Parse(const uint8_t* data, size_t size) {
    if (size != kValueSizeBytes)
        return false;
    transport_sequence_number_ = ByteReader<uint16_t>::ReadBigEndian(data);
    return true;
}

bool TransportSequenceNumber::PackInto(uint8_t* data, size_t size) const {
    if(size != kValueSizeBytes) return false;
    ByteWriter<uint16_t>::WriteBigEndian(data, transport_sequence_number_);
    return true;
}

// Video playout delay
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | len=2 |   MIN delay           |   MAX delay           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
PlayoutDelayLimits::PlayoutDelayLimits() 
    : PlayoutDelayLimits(-1, -1) {}

PlayoutDelayLimits::PlayoutDelayLimits(int min_ms, int max_ms) 
    : min_ms_(min_ms), max_ms_(max_ms) {}

PlayoutDelayLimits::~PlayoutDelayLimits() {}

bool PlayoutDelayLimits::Parse(const uint8_t* data, size_t size) {
    if (size != kValueSizeBytes) return false;
    uint32_t raw = ByteReader<uint32_t, 3>::ReadBigEndian(data);
    uint16_t min_raw = (raw >> 12);
    uint16_t max_raw = (raw & 0xfff);
    if (min_raw > max_raw)
        return false;
    min_ms_ = min_raw * kGranularityMs;
    max_ms_ = max_raw * kGranularityMs;
    return true;
}

bool PlayoutDelayLimits::PackInto(uint8_t* data, size_t size) const {
    if(size != kValueSizeBytes) return false;
    if(0 > min_ms_) return false;
    if(min_ms_ > max_ms_) return false;
    if(max_ms_ > kMaxMs) return false;
    // Convert MS to value to be sent on extension header.
    uint32_t min_delay = min_ms_ / kGranularityMs;
    uint32_t max_delay = max_ms_ / kGranularityMs;
    ByteWriter<uint32_t, 3>::WriteBigEndian(data, (min_delay << 12) | max_delay);
    return true;
}

// BaseRtpString
BaseRtpString::BaseRtpString() {}

BaseRtpString::BaseRtpString(const std::string value) 
    : value_(std::move(value)){}

BaseRtpString::~BaseRtpString() {}

bool BaseRtpString::Parse(const uint8_t* data, size_t size) {
    if (size == 0 || data[0] == 0)  // Valid string extension can't be empty.
        return false;
    const char* cstr = reinterpret_cast<const char*>(data);
    // If there is a \0 character in the middle of the |data|, treat it as end
    // of the string. Well-formed string extensions shouldn't contain it.
    value_.assign(cstr, strnlen(cstr, size));
    if(value_.empty()) {
        return false;
    }
    return true;
}

bool BaseRtpString::PackInto(uint8_t* data, size_t size) const {
    if (value_.size() > kMaxValueSizeBytes) {
        return false;
    }
    if(value_.empty() || size != value_.size())
        return false;
    memcpy(data, value_.data(), value_.size());
    return true;
}

// RtpMid
RtpMid::RtpMid() 
    : BaseRtpString() {}

RtpMid::RtpMid(const std::string value) 
    : BaseRtpString(std::move(value)) {}

RtpMid::~RtpMid() {}

} // namespace rtp
} // namespace naivertc
