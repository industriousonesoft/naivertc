#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

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

bool AbsoluteSendTime::Parse(ArrayView<const uint8_t> data, uint32_t* time_24bits) {
    if (data.size() != kValueSizeBytes)
        return false;
    *time_24bits = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
    return true;
}

bool AbsoluteSendTime::Write(ArrayView<uint8_t> data, uint32_t time_24bits) {
    if (data.size() != kValueSizeBytes) return false;
    if (time_24bits > 0x00FFFFFF) return false;
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
bool AbsoluteCaptureTime::Parse(ArrayView<const uint8_t> data, TimeInfo* time_info) {
    if (data.size() != kValueSizeBytes &&
        data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        return false;
    }

    time_info->absolute_capture_timestamp = ByteReader<uint64_t>::ReadBigEndian(data.data());

    if (data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        time_info->estimated_capture_clock_offset = ByteReader<int64_t>::ReadBigEndian(data.data() + 8);
    }

    return true;
}

bool AbsoluteCaptureTime::Write(ArrayView<uint8_t> data, const TimeInfo& time_info) {
    if (data.size() != ValueSize(time_info)) 
        return false;

    ByteWriter<uint64_t>::WriteBigEndian(data.data(), time_info.absolute_capture_timestamp);

    if (data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
        ByteWriter<int64_t>::WriteBigEndian(data.data() + 8, time_info.estimated_capture_clock_offset.value());
    }

    return true;
}

size_t AbsoluteCaptureTime::ValueSize(const TimeInfo& time_info) {
    if (time_info.estimated_capture_clock_offset != std::nullopt) {
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


bool TransmissionTimeOffset::Parse(ArrayView<const uint8_t> data, int32_t* rtp_time_24bits) {
    if (data.size() != kValueSizeBytes) return false;
    *rtp_time_24bits = ByteReader<int32_t, 3>::ReadBigEndian(data.data());
    return true;
}

bool TransmissionTimeOffset::Write(ArrayView<uint8_t> data, int32_t rtp_time_24bits) {
    if (data.size() != kValueSizeBytes) return false;
    if (rtp_time_24bits > 0x00ffffff) return false;
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
bool TransportSequenceNumber::Parse(ArrayView<const uint8_t> data, uint16_t* transport_sequence_number) {
    if (data.size() != kValueSizeBytes)
        return false;
    *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());
    return true;
}

bool TransportSequenceNumber::Write(ArrayView<uint8_t> data, uint16_t transport_sequence_number) {
    if (data.size() != kValueSizeBytes) return false;
    ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);
    return true;
}

// TransportSequenceNumberV2
//
// In addition to the format used for TransportSequencNumber, V2 also supports
// the following packet format where two extra bytes are used to specify that
// the sender requests immediate feedback.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=3   |transport-wide sequence number |T|  seq count  |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |seq count cont.|
//  +-+-+-+-+-+-+-+-+
//
// The bit `T` determines whether the feedback should include timing information
// or not and `seq_count` determines how many packets the feedback packet should
// cover including the current packet. If `seq_count` is zero no feedback is
// requested.
bool TransportSequenceNumberV2::Parse(ArrayView<const uint8_t> data,
                                      uint16_t* transport_sequence_number,
                                      std::optional<FeedbackRequest>* feedback_request) {
    if (data.size() != kValueSizeBytes &&
        data.size() != kValueSizeBytesWithoutFeedbackRequest)
        return false;

    *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());

    *feedback_request = std::nullopt;
    if (data.size() == kValueSizeBytes) {
        uint16_t feedback_request_raw =
            ByteReader<uint16_t>::ReadBigEndian(data.data() + 2);
        bool include_timestamps =
            (feedback_request_raw & kIncludeTimestampsBit) != 0;
        uint16_t sequence_count = feedback_request_raw & ~kIncludeTimestampsBit;

        // If `sequence_count` is zero no feedback is requested.
        if (sequence_count != 0) {
        *feedback_request = {include_timestamps, sequence_count};
        }
    }
    return true;
}

bool TransportSequenceNumberV2::Write(ArrayView<uint8_t> data,
                                         uint16_t transport_sequence_number,
                                         const std::optional<FeedbackRequest>& feedback_request) {
  if(data.size() != ValueSize(transport_sequence_number, feedback_request)) {
      return false;
  }

  ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);

  if (feedback_request) {
    assert(feedback_request->sequence_count >= 0);
    assert(feedback_request->sequence_count < kIncludeTimestampsBit);
    uint16_t feedback_request_raw =
        feedback_request->sequence_count |
        (feedback_request->include_timestamps ? kIncludeTimestampsBit : 0);
    ByteWriter<uint16_t>::WriteBigEndian(data.data() + 2, feedback_request_raw);
  }
  return true;
}

// Video playout delay
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | len=2 |   MIN delay           |   MAX delay           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
bool PlayoutDelayLimits::Parse(ArrayView<const uint8_t> data, ValueType* playout_delay) {
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

bool PlayoutDelayLimits::Write(ArrayView<uint8_t> data, const ValueType& playout_delay) {
    if (data.size() != kValueSizeBytes) return false;
    if (0 > playout_delay.min_ms) return false;
    if (playout_delay.min_ms > playout_delay.max_ms) return false;
    if (playout_delay.max_ms > kMaxMs) return false;
    // Convert MS to value to be sent on extension header.
    uint32_t min_delay = playout_delay.min_ms / kGranularityMs;
    uint32_t max_delay = playout_delay.max_ms / kGranularityMs;
    ByteWriter<uint32_t, 3>::WriteBigEndian(data.data(), (min_delay << 12) | max_delay);
    return true;
}

// BaseRtpString
bool BaseRtpString::Parse(ArrayView<const uint8_t> data, std::string* value) {
    if (data.size() == 0 || data.data()[0] == 0)  // Valid string extension can't be empty.
        return false;
    const char* cstr = reinterpret_cast<const char*>(data.data());
    // If there is a \0 character in the middle of the |data|, treat it as end
    // of the string. Well-formed string extensions shouldn't contain it.
    value->assign(cstr, strnlen(cstr, data.size()));
    if (value->empty()) {
        return false;
    }
    return true;
}

bool BaseRtpString::Write(ArrayView<uint8_t> data, const std::string& value) {
    if (value.size() > kMaxValueSizeBytes) {
        return false;
    }
    if (value.empty() || data.size() != value.size())
        return false;
    memcpy(data.data(), value.data(), value.size());
    return true;
}

} // namespace rtp
} // namespace naivertc
