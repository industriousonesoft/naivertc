#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
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

bool AbsoluteSendTime::Parse(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size != kValueSizeBytes)
        return false;
    time_24bits_ = ByteReader<uint32_t, 3>::ReadBigEndian(buffer);
    return true;
}

bool AbsoluteSendTime::PackInto(uint8_t* buffer, size_t buffer_size) const {
    return PackInto(buffer, buffer_size, time_24bits_);
}

bool AbsoluteSendTime::PackInto(uint8_t* buffer, size_t buffer_size, uint32_t time_24bits) {
    if(buffer_size != kValueSizeBytes) return false;
    if(time_24bits > 0x00FFFFFF) return false;
    ByteWriter<uint32_t, 3>::WriteBigEndian(buffer, time_24bits);
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

size_t AbsoluteCaptureTime::size() const {
    if (estimated_capture_clock_offset_ != std::nullopt) {
        return kValueSizeBytes;
    } else {
        return kValueSizeBytesWithoutEstimatedCaptureClockOffset;
    }
}

bool AbsoluteCaptureTime::PackInto(uint8_t* data, size_t size) const {
    if(size != this->size()) 
        return false;

    ByteWriter<uint64_t>::WriteBigEndian(data, absolute_capture_timestamp_);

    if (size != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        ByteWriter<int64_t>::WriteBigEndian(data + 8, estimated_capture_clock_offset_.value());
    }

    return true;
}

bool AbsoluteCaptureTime::PackInto(uint8_t* buffer, size_t buffer_size, 
                                   uint64_t absolute_capture_timestamp,
                                   std::optional<int64_t> estimated_capture_clock_offset) {
    if (ValueSize(absolute_capture_timestamp, estimated_capture_clock_offset) != buffer_size) {
        return false;
    }

    ByteWriter<uint64_t>::WriteBigEndian(buffer, absolute_capture_timestamp);

    if (buffer_size != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        ByteWriter<int64_t>::WriteBigEndian(buffer + 8, estimated_capture_clock_offset.value());
    }

    return true;
}

size_t AbsoluteCaptureTime::ValueSize(uint64_t absolute_capture_timestamp,
                                      std::optional<int64_t> estimated_capture_clock_offset) {
    if (estimated_capture_clock_offset != std::nullopt) {
        return kValueSizeBytes;
    } else {
        return kValueSizeBytesWithoutEstimatedCaptureClockOffset;
    }                          
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

bool TransmissionTimeOffset::Parse(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size != kValueSizeBytes) return false;
    rtp_time_24bits_ = ByteReader<int32_t, 3>::ReadBigEndian(buffer);
    return true;
}

bool TransmissionTimeOffset::PackInto(uint8_t* buffer, size_t buffer_size) const {
    return TransmissionTimeOffset::PackInto(buffer, buffer_size, rtp_time_24bits_);
}

bool TransmissionTimeOffset::PackInto(uint8_t* buffer, size_t buffer_size, int32_t rtp_time_24bits) {
    if(buffer_size != kValueSizeBytes) return false;
    if(rtp_time_24bits > 0x00ffffff) return false;
    ByteWriter<int32_t, 3>::WriteBigEndian(buffer, rtp_time_24bits);
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

bool TransportSequenceNumber::Parse(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size != kValueSizeBytes)
        return false;
    transport_sequence_number_ = ByteReader<uint16_t>::ReadBigEndian(buffer);
    return true;
}

bool TransportSequenceNumber::PackInto(uint8_t* buffer, size_t buffer_size) const {
    return TransportSequenceNumber::PackInto(buffer, buffer_size, transport_sequence_number_);
}

bool TransportSequenceNumber::PackInto(uint8_t* buffer, size_t buffer_size, uint16_t transport_sequence_number) {
    if(buffer_size != kValueSizeBytes) return false;
    ByteWriter<uint16_t>::WriteBigEndian(buffer, transport_sequence_number);
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

bool PlayoutDelayLimits::Parse(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size != kValueSizeBytes) return false;
    uint32_t raw = ByteReader<uint32_t, 3>::ReadBigEndian(buffer);
    uint16_t min_raw = (raw >> 12);
    uint16_t max_raw = (raw & 0xfff);
    if (min_raw > max_raw)
        return false;
    min_ms_ = min_raw * kGranularityMs;
    max_ms_ = max_raw * kGranularityMs;
    return true;
}

bool PlayoutDelayLimits::PackInto(uint8_t* buffer, size_t buffer_size) const {
    return PlayoutDelayLimits::PackInto(buffer, buffer_size, min_ms_, max_ms_);
} 

bool PlayoutDelayLimits::PackInto(uint8_t* buffer, size_t buffer_size, int min_ms, int max_ms) {
    if(buffer_size != kValueSizeBytes) return false;
    if(0 > min_ms) return false;
    if(min_ms > max_ms) return false;
    if(max_ms > kMaxMs) return false;
    // Convert MS to value to be sent on extension header.
    uint32_t min_delay = min_ms / kGranularityMs;
    uint32_t max_delay = max_ms / kGranularityMs;
    ByteWriter<uint32_t, 3>::WriteBigEndian(buffer, (min_delay << 12) | max_delay);
    return true;
}

// BaseRtpString
BaseRtpString::BaseRtpString() {}

BaseRtpString::BaseRtpString(const std::string value) 
    : value_(std::move(value)){}

BaseRtpString::~BaseRtpString() {}

bool BaseRtpString::Parse(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size == 0 || buffer[0] == 0)  // Valid string extension can't be empty.
        return false;
    const char* cstr = reinterpret_cast<const char*>(buffer);
    // If there is a \0 character in the middle of the |data|, treat it as end
    // of the string. Well-formed string extensions shouldn't contain it.
    value_.assign(cstr, strnlen(cstr, buffer_size));
    if(value_.empty()) {
        return false;
    }
    return true;
}

bool BaseRtpString::PackInto(uint8_t* buffer, size_t buffer_size) const {
    return BaseRtpString::PackInto(buffer, buffer_size, value_);
}

bool BaseRtpString::PackInto(uint8_t* buffer, size_t buffer_size, const std::string value) {
    if (value.size() > kMaxValueSizeBytes) {
        return false;
    }
    if(value.empty() || buffer_size != value.size())
        return false;
    memcpy(buffer, value.data(), value.size());
    return true;
}

// RtpMid
RtpMid::RtpMid() 
    : BaseRtpString() {}

RtpMid::RtpMid(const std::string value) 
    : BaseRtpString(std::move(value)) {}

RtpMid::~RtpMid() {}

// RtpStreamId
RtpStreamId::RtpStreamId() 
    : BaseRtpString() {}

RtpStreamId::RtpStreamId(const std::string value) 
    : BaseRtpString(std::move(value)) {}

RtpStreamId::~RtpStreamId() {}

// RepairedRtpStreamId
RepairedRtpStreamId::RepairedRtpStreamId() 
    : BaseRtpString() {}

RepairedRtpStreamId::RepairedRtpStreamId(const std::string value) 
    : BaseRtpString(std::move(value)) {}

RepairedRtpStreamId::~RepairedRtpStreamId() {}

} // namespace rtp
} // namespace naivertc
