#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

namespace naivertc {
    
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

bool AbsoluteSendTimeExtension::Parse(std::vector<const uint8_t> data,
                             uint32_t* time_24bits) {
  if (data.size() != kValueSizeBytes)
    return false;
  *time_24bits = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
  return true;
}

bool AbsoluteSendTimeExtension::Write(std::vector<uint8_t> data,
                             uint32_t time_24bits) {
  if(data.size() != kValueSizeBytes) return false;
  if(time_24bits > 0x00FFFFFF) return false;
  ByteWriter<uint32_t, 3>::WriteBigEndian(data.data(), time_24bits);
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

bool AbsoluteCaptureTimeExtension::Parse(std::vector<const uint8_t> data, AbsoluteCaptureTime* absolute_capture_time) {
    if (absolute_capture_time == nullptr) return false;
    if (data.size() != kValueSizeBytes || /*FIXME: WebRTC use && here? */
        data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        return false;
    }

    absolute_capture_time->absolute_capture_timestamp = ByteReader<uint64_t>::ReadBigEndian(data.data());

    if (data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        absolute_capture_time->estimated_capture_clock_offset = ByteReader<int64_t>::ReadBigEndian(data.data() + 8);
    }

    return true;
}

size_t AbsoluteCaptureTimeExtension::ValueSize(const AbsoluteCaptureTime& absolute_capture_time) {
    if (absolute_capture_time.estimated_capture_clock_offset != std::nullopt) {
        return kValueSizeBytes;
    } else {
        return kValueSizeBytesWithoutEstimatedCaptureClockOffset;
    }
}

bool AbsoluteCaptureTimeExtension::Write(std::vector<uint8_t> data, const AbsoluteCaptureTime& absolute_capture_time) {
    if(data.size() != ValueSize(absolute_capture_time)) return false;

    ByteWriter<uint64_t>::WriteBigEndian(data.data(), absolute_capture_time.absolute_capture_timestamp);

    if (data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        ByteWriter<int64_t>::WriteBigEndian(data.data() + 8, absolute_capture_time.estimated_capture_clock_offset.value());
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
bool TransmissionOffsetExtension::Parse(std::vector<const uint8_t> data, int32_t* rtp_time_24bits) {
    if (data.size() != kValueSizeBytes) return false;
    *rtp_time_24bits = ByteReader<int32_t, 3>::ReadBigEndian(data.data());
    return true;
}

bool TransmissionOffsetExtension::Write(std::vector<uint8_t> data, int32_t rtp_time_24bits) {
    if(data.size() != kValueSizeBytes) return false;
    if(rtp_time_24bits > 0x00ffffff) return false;
    ByteWriter<int32_t, 3>::WriteBigEndian(data.data(), rtp_time_24bits);
    return true;
}

// TransportSequenceNumber
//
//   0                   1                   2
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=1   |transport-wide sequence number |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

bool TransportSequenceNumberExtension::Parse(std::vector<const uint8_t> data, uint16_t* transport_sequence_number) {
    if (data.size() != kValueSizeBytes)
        return false;
    *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());
    return true;
}

bool TransportSequenceNumberExtension::Write(std::vector<uint8_t> data, uint16_t transport_sequence_number) {
    if(data.size() != kValueSizeBytes) return false;
    ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);
    return true;
}

// Video playout delay
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | len=2 |   MIN delay           |   MAX delay           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

bool PlayoutDelayLimitsExtension::Parse(std::vector<const uint8_t> data, VideoPlayoutDelay* playout_delay) {
    if (playout_delay == nullptr) return false;
    if (data.size() != kValueSizeBytes) return false;
    uint32_t raw = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
    uint16_t min_raw = (raw >> 12);
    uint16_t max_raw = (raw & 0xfff);
    if (min_raw > max_raw)
        return false;
    playout_delay->min_ms = min_raw * kGranularityMs;
    playout_delay->max_ms = max_raw * kGranularityMs;
    return true;
}

bool PlayoutDelayLimitsExtension::Write(std::vector<uint8_t> data, const VideoPlayoutDelay& playout_delay) {
    if(data.size() != kValueSizeBytes) return false;
    if(0 > playout_delay.min_ms) return false;
    if(playout_delay.min_ms > playout_delay.max_ms) return false;
    if(playout_delay.max_ms > kMaxMs) return false;
    // Convert MS to value to be sent on extension header.
    uint32_t min_delay = playout_delay.min_ms / kGranularityMs;
    uint32_t max_delay = playout_delay.max_ms / kGranularityMs;
    ByteWriter<uint32_t, 3>::WriteBigEndian(data.data(),
                                            (min_delay << 12) | max_delay);
    return true;
}

// BaseRtpString
bool BaseRtpStringExtension::Parse(std::vector<const uint8_t> data, std::string* str) {
    if (data.empty() || data[0] == 0)  // Valid string extension can't be empty.
        return false;
    const char* cstr = reinterpret_cast<const char*>(data.data());
    // If there is a \0 character in the middle of the |data|, treat it as end
    // of the string. Well-formed string extensions shouldn't contain it.
    str->assign(cstr, strnlen(cstr, data.size()));
    if(str->empty()) {
        return false;
    }
    return true;
}

bool BaseRtpStringExtension::Write(std::vector<uint8_t> data, const std::string& str) {
    if (str.size() > kMaxValueSizeBytes) {
        return false;
    }
    if(str.empty() || data.size() != str.size())
        return false;
    memcpy(data.data(), str.data(), str.size());
    return true;
}

} // namespace naivertc
