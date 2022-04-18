#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {
namespace {
    
// See https://blog.jianchihu.net/webrtc-research-transport-cc-rtp-rtcp.html
// Header size:
// * 4 bytes Common RTCP Packet Header
// * 8 bytes Common Packet Format for RTCP Feedback Messages
// * 8 bytes FeedbackPacket header
constexpr size_t kTransportFeedbackHeaderSizeBytes = 4 + 8 + 8;
constexpr size_t kChunkSizeBytes = 2;
// TODO(sprang): Add support for dynamic max size for easier fragmentation,
// eg. set it to what's left in the buffer or IP_PACKET_SIZE.
// Size constraint imposed by RTCP common header: 16bit size field interpreted
// as number of four byte words minus the first header word.
constexpr size_t kMaxSizeBytes = (1 << 16) * 4;
// Payload size:
// * 8 bytes Common Packet Format for RTCP Feedback Messages
// * 8 bytes FeedbackPacket header.
// * 2 bytes for one chunk.
constexpr size_t kMinPayloadSizeBytes = 8 + 8 + 2;
constexpr int kBaseScaleFactor = TransportFeedback::kDeltaScaleFactor * (1 << 8);
constexpr int64_t kTimeWrapPeriodUs = (1ll << 24) * kBaseScaleFactor;

} // namespace

//    Message format
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P|  FMT=15 |    PT=205     |           length              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                     SSRC of packet sender                     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                      SSRC of media source                     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |      base sequence number     |      packet status count      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |                 reference time                | fb pkt. count |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |          packet chunk         |         packet chunk          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    .                                                               .
//    .                                                               .
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |         packet chunk          |  recv delta   |  recv delta   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    .                                                               .
//    .                                                               .
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |           recv delta          |  recv delta   | zero padding  |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

constexpr uint8_t TransportFeedback::kFeedbackMessageType;
constexpr size_t TransportFeedback::kMaxReportedPackets;

TransportFeedback::TransportFeedback()
    : TransportFeedback(/*include_timestamps=*/true, /*include_lost=*/true) {}

TransportFeedback::TransportFeedback(bool include_timestamps, bool include_lost)
    : include_lost_(include_lost),
      base_seq_num_(0),
      status_count_(0),
      refernce_time_(0),
      feedback_seq_(0),
      include_timestamps_(include_timestamps),
      last_timestamp_us_(0),
      size_bytes_(kTransportFeedbackHeaderSizeBytes) {}

TransportFeedback::TransportFeedback(const TransportFeedback&) = default;

TransportFeedback::TransportFeedback(TransportFeedback&& other)
    : include_lost_(other.include_lost_),
      base_seq_num_(other.base_seq_num_),
      status_count_(other.status_count_),
      refernce_time_(other.refernce_time_),
      feedback_seq_(other.feedback_seq_),
      include_timestamps_(other.include_timestamps_),
      last_timestamp_us_(other.last_timestamp_us_),
      received_packets_(std::move(other.received_packets_)),
      all_packets_(std::move(other.all_packets_)),
      encoded_chunks_(std::move(other.encoded_chunks_)),
      last_chunk_(other.last_chunk_),
      size_bytes_(other.size_bytes_) {
    other.Clear();
}

TransportFeedback::~TransportFeedback() {}

void TransportFeedback::SetBase(uint16_t base_sequence,
                                int64_t ref_timestamp_us) {
    assert(status_count_ == 0);
    assert(ref_timestamp_us >= 0);
    base_seq_num_ = base_sequence;
    refernce_time_ = (ref_timestamp_us % kTimeWrapPeriodUs) / kBaseScaleFactor;
    last_timestamp_us_ = GetBaseTimeUs();
}

void TransportFeedback::SetFeedbackSequenceNumber(uint8_t feedback_sequence) {
    feedback_seq_ = feedback_sequence;
}

bool TransportFeedback::AddReceivedPacket(uint16_t sequence_number,
                                          int64_t timestamp_us) {
    // Set delta to zero if timestamps are not included, this will simplify the
    // encoding process.
    int16_t delta = 0;
    if (include_timestamps_) {
        // Convert to ticks and round.
        int64_t delta_full = (timestamp_us - last_timestamp_us_) % kTimeWrapPeriodUs;
        if (delta_full > kTimeWrapPeriodUs / 2) {
            delta_full -= kTimeWrapPeriodUs;
        }
        delta_full += delta_full < 0 ? -(kDeltaScaleFactor / 2) : kDeltaScaleFactor / 2;
        delta_full /= kDeltaScaleFactor;

        delta = static_cast<int16_t>(delta_full);
        // If larger than 16bit signed, we can't represent it - need new fb packet.
        if (delta != delta_full) {
            PLOG_WARNING << "Delta value too large ( >= 2^16 ticks )";
            return false;
        }
    }

    uint16_t next_seq_no = base_seq_num_ + status_count_;
    if (sequence_number != next_seq_no) {
        uint16_t last_seq_no = next_seq_no - 1;
        if (!wrap_around_utils::AheadOf(sequence_number, last_seq_no)) {
            return false;
        }
        for (; next_seq_no != sequence_number; ++next_seq_no) {
            if (!AddDeltaSize(0)) {
                return false;
            }
            if (include_lost_) {
                all_packets_.emplace_back(next_seq_no);
            }
        }
    }

    DeltaSize delta_size = (delta >= 0 && delta <= 0xff) ? 1 : 2;
    if (!AddDeltaSize(delta_size))
        return false;

    received_packets_.emplace_back(sequence_number, delta);
    if (include_lost_)
        all_packets_.emplace_back(sequence_number, delta);
    last_timestamp_us_ += delta * kDeltaScaleFactor;
    if (include_timestamps_) {
        size_bytes_ += delta_size;
    }
    return true;
}

const std::vector<TransportFeedback::ReceivedPacket>&
TransportFeedback::GetReceivedPackets() const {
    return received_packets_;
}

const std::vector<TransportFeedback::ReceivedPacket>&
TransportFeedback::GetAllPackets() const {
    assert(include_lost_);
    return all_packets_;
}

uint16_t TransportFeedback::GetBaseSequence() const {
    return base_seq_num_;
}

int64_t TransportFeedback::GetBaseTimeUs() const {
    return static_cast<int64_t>(refernce_time_) * kBaseScaleFactor;
}

TimeDelta TransportFeedback::GetBaseTime() const {
    return TimeDelta::Micros(GetBaseTimeUs());
}

int64_t TransportFeedback::GetBaseDeltaUs(int64_t prev_timestamp_us) const {
    int64_t delta = GetBaseTimeUs() - prev_timestamp_us;

    // Detect and compensate for wrap-arounds in base time.
    if (std::abs(delta - kTimeWrapPeriodUs) < std::abs(delta)) {
        delta -= kTimeWrapPeriodUs;  // Wrap backwards.
    } else if (std::abs(delta + kTimeWrapPeriodUs) < std::abs(delta)) {
        delta += kTimeWrapPeriodUs;  // Wrap forwards.
    }
    return delta;
}

TimeDelta TransportFeedback::GetBaseDelta(TimeDelta prev_timestamp) const {
    return TimeDelta::Micros(GetBaseDeltaUs(prev_timestamp.us()));
}

// De-serialize packet.
bool TransportFeedback::Parse(const CommonHeader& packet) {
    assert(packet.type() == kPacketType);
    assert(packet.feedback_message_type() == kFeedbackMessageType);
 
    if (packet.payload_size() < kMinPayloadSizeBytes) {
        PLOG_WARNING << "Buffer too small (" 
                    << packet.payload_size()
                    << " bytes) to fit a FeedbackPacket. Minimum size = "
                    << kMinPayloadSizeBytes;
        return false;
    }

    // Rtp feedback common header
    ParseCommonFeedback(packet.payload(), packet.payload_size());

    const uint8_t* const payload = packet.payload();

    // Base sequence number (16 bits)
    base_seq_num_ = ByteReader<uint16_t>::ReadBigEndian(&payload[8]);
    // Packet status count (16 bits)
    uint16_t status_count = ByteReader<uint16_t>::ReadBigEndian(&payload[10]);
    // Reference time (24 bits)
    refernce_time_ = ByteReader<int32_t, 3>::ReadBigEndian(&payload[12]);
    // feedback packet count
    feedback_seq_ = payload[15];
    Clear();
    size_t index = 16;
    const size_t end_index = packet.payload_size();

    if (status_count == 0) {
        PLOG_WARNING << "Empty feedback messages not allowed.";
        return false;
    }

    std::vector<uint8_t> delta_sizes;
    delta_sizes.reserve(status_count);
    while (delta_sizes.size() < status_count) {
        if (index + kChunkSizeBytes > end_index) {
            PLOG_WARNING << "Buffer overflow while parsing packet.";
            Clear();
            return false;
        }
        uint16_t chunk = ByteReader<uint16_t>::ReadBigEndian(&payload[index]);
        index += kChunkSizeBytes;
        encoded_chunks_.push_back(chunk);
        last_chunk_.Decode(chunk, status_count - delta_sizes.size());
        last_chunk_.AppendTo(&delta_sizes);
    }
    // Last chunk is stored in the |last_chunk_|.
    encoded_chunks_.pop_back();
    assert(delta_sizes.size() == status_count);
    status_count_ = status_count;

    uint16_t seq_no = base_seq_num_;
    size_t recv_delta_size = 0;
    for (size_t delta_size : delta_sizes) {
        recv_delta_size += delta_size;
    }

    // Determine if timestamps, that is, recv_delta are included in the packet.
    if (end_index >= index + recv_delta_size) {
        for (size_t delta_size : delta_sizes) {
            if (index + delta_size > end_index) {
                PLOG_WARNING << "Buffer overflow while parsing packet.";
                Clear();
                return false;
            }
            switch (delta_size) {
                case 0:
                    if (include_lost_) {
                        all_packets_.emplace_back(seq_no);
                    }
                    break;
                case 1: {
                    int16_t delta = payload[index];
                    received_packets_.emplace_back(seq_no, delta);
                    if (include_lost_) {
                        all_packets_.emplace_back(seq_no, delta);
                    }
                    last_timestamp_us_ += delta * kDeltaScaleFactor;
                    index += delta_size;
                    break;
                }
                case 2: {
                    int16_t delta = ByteReader<int16_t>::ReadBigEndian(&payload[index]);
                    received_packets_.emplace_back(seq_no, delta);
                    if (include_lost_) {
                        all_packets_.emplace_back(seq_no, delta);
                    }
                    last_timestamp_us_ += delta * kDeltaScaleFactor;
                    index += delta_size;
                    break;
                }
                case 3: {
                    Clear();
                    PLOG_WARNING << "Invalid delta_size for seq_no " << seq_no;
                    return false;
                }   
                default:
                    RTC_NOTREACHED();
                break;
            }
            ++seq_no;
        }
    } else {
        // The packet does not contain receive deltas.
        include_timestamps_ = false;
        for (size_t delta_size : delta_sizes) {
            // Use delta sizes to detect if packet was received.
            if (delta_size > 0) {
                received_packets_.emplace_back(seq_no, 0);
            }
            if (include_lost_) {
                if (delta_size > 0) {
                    all_packets_.emplace_back(seq_no, 0);
                } else {
                    all_packets_.emplace_back(seq_no);
                }
            }
            ++seq_no;
        }
    }
    size_bytes_ = RtcpPacket::kRtcpCommonHeaderSize + index;
    assert(index <= end_index);
    return true;
}

std::unique_ptr<TransportFeedback> TransportFeedback::ParseFrom(const uint8_t* buffer,
                                                                size_t size) {
    CommonHeader header;
    if (!header.Parse(buffer, size))
        return nullptr;
    if (header.type() != kPacketType || header.feedback_message_type() != kFeedbackMessageType)
        return nullptr;
    std::unique_ptr<TransportFeedback> parsed(new TransportFeedback);
    if (!parsed->Parse(header))
        return nullptr;
    return parsed;
}

bool TransportFeedback::IsConsistent() const {
    size_t packet_size = kTransportFeedbackHeaderSizeBytes;
    std::vector<DeltaSize> delta_sizes;
    LastChunk chunk_decoder;
    for (uint16_t chunk : encoded_chunks_) {
        chunk_decoder.Decode(chunk, kMaxReportedPackets);
        chunk_decoder.AppendTo(&delta_sizes);
        packet_size += kChunkSizeBytes;
    }
    if (!last_chunk_.Empty()) {
        last_chunk_.AppendTo(&delta_sizes);
        packet_size += kChunkSizeBytes;
    }
    if (status_count_ != delta_sizes.size()) {
        PLOG_ERROR << delta_sizes.size() << " packets encoded. Expected "
                   << status_count_;
        return false;
    }
    int64_t timestamp_us = refernce_time_ * kBaseScaleFactor;
    auto packet_it = received_packets_.begin();
    uint16_t seq_no = base_seq_num_;
    for (DeltaSize delta_size : delta_sizes) {
        if (delta_size > 0) {
            if (packet_it == received_packets_.end()) {
                PLOG_ERROR << "Failed to find delta for seq_no " << seq_no;
                return false;
            }
            if (packet_it->sequence_number() != seq_no) {
                PLOG_ERROR << "Expected to find delta for seq_no " << seq_no
                           << ". Next delta is for "
                           << packet_it->sequence_number();
                return false;
            }
            if (delta_size == 1 &&
                (packet_it->delta_ticks() < 0 || packet_it->delta_ticks() > 0xff)) {
                PLOG_ERROR << "Delta " << packet_it->delta_ticks()
                           << " for seq_no " << seq_no
                           << " doesn't fit into one byte";
                return false;
            }
            timestamp_us += packet_it->delta_us();
            ++packet_it;
        }
        if (include_timestamps_) {
            packet_size += delta_size;
        }
        ++seq_no;
    }
    if (packet_it != received_packets_.end()) {
        PLOG_ERROR << "Unencoded delta for seq_no "
                   << packet_it->sequence_number();
        return false;
    }
    if (timestamp_us != last_timestamp_us_) {
        PLOG_ERROR << "Last timestamp mismatch. Calculated: " << timestamp_us
                   << ". Saved: " << last_timestamp_us_;
        return false;
    }
    if (size_bytes_ != packet_size) {
        PLOG_ERROR << "Rtcp packet size mismatch. Calculated: " << packet_size 
                   << ". Saved: " << size_bytes_;
        return false;
    }
    return true;
}

size_t TransportFeedback::PacketSize() const {
    // Round size_bytes_ up to multiple of 32bits.
    return (size_bytes_ + 3) & (~static_cast<size_t>(3));
}

size_t TransportFeedback::PaddingSize() const {
    return PacketSize() - size_bytes_;
}

// Serialize packet.
bool TransportFeedback::PackInto(uint8_t* buffer,
                                 size_t* index,
                                 size_t max_size,
                                 PacketReadyCallback callback) const {
    if (status_count_ == 0)
        return false;

    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }
    const size_t index_end = *index + PacketSize();
    const size_t padding_size = PaddingSize();
    bool has_padding = padding_size > 0;
    PackCommonHeader(kFeedbackMessageType, 
                     kPacketType, 
                     PacketSizeWithoutCommonHeader(), 
                     has_padding,
                     buffer, 
                     index);
    Rtpfb::PackCommonFeedbackInto(buffer + *index, index_end - *index);
    *index += kCommonFeedbackSize;

    // Base sequence number (16 bits)
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index], base_seq_num_);
    *index += 2;

    // Packet status count (16 bits)
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index], status_count_);
    *index += 2;

    // Reference time (24 bits)
    ByteWriter<int32_t, 3>::WriteBigEndian(&buffer[*index], refernce_time_);
    *index += 3;

    // Feedback pkt count (8 bits)
    buffer[(*index)++] = feedback_seq_;

    // Chunks
    for (uint16_t chunk : encoded_chunks_) {
        ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index], chunk);
        *index += 2;
    }

    // Last chunk
    if (!last_chunk_.Empty()) {
        uint16_t chunk = last_chunk_.EncodeLast();
        ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index], chunk);
        *index += 2;
    }

    // Timestamps
    if (include_timestamps_) {
        for (const auto& received_packet : received_packets_) {
            int16_t delta = received_packet.delta_ticks();
            if (delta >= 0 && delta <= 0xFF) {
                buffer[(*index)++] = delta;
            } else {
                ByteWriter<int16_t>::WriteBigEndian(&buffer[*index], delta);
                *index += 2;
            }
        }
    }

    // Padding
    if (padding_size > 0) {
        for (size_t i = 0; i < padding_size - 1; ++i) {
            buffer[(*index)++] = 0;
        }
        buffer[(*index)++] = padding_size;
    }
    assert(*index == index_end);
    return true;
}

void TransportFeedback::Clear() {
    status_count_ = 0;
    last_timestamp_us_ = GetBaseTimeUs();
    received_packets_.clear();
    all_packets_.clear();
    encoded_chunks_.clear();
    last_chunk_.Clear();
    size_bytes_ = kTransportFeedbackHeaderSizeBytes;
}

bool TransportFeedback::AddDeltaSize(DeltaSize delta_size) {
    if (status_count_ == kMaxReportedPackets)
        return false;
    size_t add_chunk_size = last_chunk_.Empty() ? kChunkSizeBytes : 0;
    if (size_bytes_ + delta_size + add_chunk_size > kMaxSizeBytes)
        return false;

    if (last_chunk_.CanAdd(delta_size)) {
        size_bytes_ += add_chunk_size;
        last_chunk_.Add(delta_size);
        ++status_count_;
        return true;
    }
    if (size_bytes_ + delta_size + kChunkSizeBytes > kMaxSizeBytes)
        return false;

    encoded_chunks_.push_back(last_chunk_.Emit());
    size_bytes_ += kChunkSizeBytes;
    last_chunk_.Add(delta_size);
    ++status_count_;
    return true;
}
    
} // namespace rtcp    
} // namespace naivertc
