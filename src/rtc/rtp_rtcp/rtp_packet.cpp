#include "rtc/rtp_rtcp/rtp_packet.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

namespace naivertc {

namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kOneByteExtensionProfiledId = 0xBEDE;
constexpr uint16_t kTwoByteExtensionProfiledId = 0x1000;
constexpr uint16_t kTwoByteExtensionProfiledIdAppBitsFilter = 0xFFF0;
constexpr size_t kOneByteExtensionHeaderLength = 1;
constexpr size_t kTwoByteExtensionHeaderLength = 2;
} // namespace

/* RTP packet
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier            |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|            Contributing source (CSRC) identifiers             |
|                             ....                              |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|  header eXtension profile id  |       length in 32bits        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Extensions                           |
|                             ....                              |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                           Payload                             |
|             ....              :  padding...                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               padding         | Padding size  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

RtpPacket::RtpPacket() 
    : RtpPacket(kDefaultPacketSize) {}

RtpPacket::RtpPacket(size_t capacity) 
    : Packet(capacity) {
    Clear();
}
    
RtpPacket::~RtpPacket() {
}

std::vector<uint32_t> RtpPacket::csrcs() const {
    size_t num_csrc = data()[0] & 0x0F;
    assert((kFixedHeaderSize + num_csrc * 4) <= capacity());
    std::vector<uint32_t> csrcs(num_csrc);
    size_t offset = kFixedHeaderSize;
    for (size_t i = 0; i < num_csrc; ++i) {
        csrcs[i] = ByteReader<uint32_t>::ReadBigEndian(&data()[offset]);
        offset += 4;
    }
    return csrcs;
}

void RtpPacket::set_marker(bool marker) {
    marker_ = marker;
    if (marker_) {
        WriteAt(1, data()[1] | 0x80);
    }else {
        WriteAt(1, data()[1] & 0x7F);
    }
}

void RtpPacket::set_payload_type(uint8_t payload_type) {
    assert(payload_type <= 0x7Fu && "Invalid payload type");
    payload_type_ = payload_type;
    WriteAt(1, (data()[1] & 0x80) | payload_type);
}

void RtpPacket::set_sequence_number(uint16_t sequence_num) {
    sequence_num_ = sequence_num;
    ByteWriter<uint16_t>::WriteBigEndian(WriteAt(2), sequence_num_);
}

void RtpPacket::set_timestamp(uint32_t timestamp) {
    timestamp_ = timestamp;
    ByteWriter<uint32_t>::WriteBigEndian(WriteAt(4), timestamp_);
}

void RtpPacket::set_ssrc(uint32_t ssrc) {
    ssrc_ = ssrc;
    ByteWriter<uint32_t>::WriteBigEndian(WriteAt(8), ssrc_);
}

// Write csrc list, Assumes:
// a) There is enough room left in buffer.
// b) Extension headers, payload or padding data has not already been added.
void RtpPacket::SetCsrcs(std::vector<uint32_t> csrcs) {
    assert(extensions_size_ == 0);
    assert(payload_size_ == 0);
    assert(padding_size_ == 0);
    assert(csrcs.size() <= 0x0Fu);
    assert(kFixedHeaderSize + 4 * csrcs.size() <= capacity());

    payload_offset_ = kFixedHeaderSize + 4 * csrcs.size();
    WriteAt(0, (data()[0] & 0xF0) | static_cast<uint8_t>(csrcs.size()));
    size_t offset = kFixedHeaderSize;
    for (uint32_t csrc : csrcs) {
        ByteWriter<uint32_t>::WriteBigEndian(WriteAt(offset), csrc);
        offset += 4;
    }
    BinaryBuffer::resize(payload_offset_);
}

void RtpPacket::CopyHeaderFrom(const RtpPacket& other) {
    marker_ = other.marker_;
    payload_type_ = other.payload_type_;
    sequence_num_ = other.sequence_num_;
    timestamp_ = other.timestamp_;
    ssrc_ = other.ssrc_;
    payload_offset_ = other.payload_offset_;

    BinaryBuffer::clear();
    BinaryBuffer::assign(other.begin(), other.begin() + other.header_size());

    // Reset payload and padding
    payload_size_ = 0;
    padding_size_ = 0;
}

void RtpPacket::Clear() {
    marker_ = false;
    payload_type_ = 0;
    sequence_num_ = 0;
    timestamp_ = 0;
    ssrc_ = 0;
    payload_offset_ = kFixedHeaderSize;
    payload_size_ = 0;
    padding_size_ = 0;

    // After clear, size changes to 0 and capacity stays the same.
    BinaryBuffer::clear();
    BinaryBuffer::resize(kFixedHeaderSize);
    WriteAt(0, kRtpVersion << 6);
}

inline void RtpPacket::WriteAt(size_t offset, uint8_t byte) {
    BinaryBuffer::at(offset) = byte;
}

inline uint8_t* RtpPacket::WriteAt(size_t offset) {
    assert(offset <= capacity() && "Out of bounds");
    return BinaryBuffer::data() + offset;
}

// Parse
bool RtpPacket::Parse(const uint8_t* buffer, size_t size) {
    if (size < kFixedHeaderSize) {
        return false;
    }
    const uint8_t version = buffer[0] >> 6;
    if (version != kRtpVersion) {
        return false;
    }
    const bool has_padding = (buffer[0] & 0x20) != 0;
    const bool has_extension = (buffer[0] & 0x10) != 0;
    const uint8_t number_of_csrcs = buffer[0] & 0x0F;
    size_t payload_offset = kFixedHeaderSize + number_of_csrcs * 4;
    if (size < payload_offset) {
        return false;
    }
    marker_ = (buffer[1] & 0x80) != 0;
    payload_type_ = buffer[1] & 0x7F;

    sequence_num_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
    timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
    ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
    
    payload_offset_ = payload_offset;
    extensions_size_ = 0;
    if (has_extension) {
        /* RTP header extension, RFC 3550.
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |      defined by profile       |           length              |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                        header extension                       |
        |                             ....                              |
        */
       size_t extension_offset = payload_offset_ + 4;
       if (extension_offset > size) {
           return false;
       }
       uint16_t profile_id = ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_]);
       size_t extension_capacity = ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_ + 2]);
       extension_capacity *= 4;
       if (extension_offset + extension_capacity > size) {
           return false;
       }
       if (profile_id != kOneByteExtensionProfiledId &&
        (profile_id & kTwoByteExtensionProfiledIdAppBitsFilter) != kTwoByteExtensionProfiledId) {
            PLOG_WARNING << "Unsupported RTP extension: " << profile_id;
        }else {
            size_t extension_header_length = profile_id == kOneByteExtensionProfiledId ? kOneByteExtensionHeaderLength : kTwoByteExtensionHeaderLength;
            constexpr uint8_t kPaddingByte = 0;
            constexpr uint8_t kPaddingId = 0;
            constexpr uint8_t kOneByteHeaderExtensionReservedId = 15;
            while (extensions_size_ + extension_header_length < extension_capacity) {
                if (buffer[extension_offset + extensions_size_] == kPaddingByte) {
                    extensions_size_++;
                    continue;
                }
                int id;
                uint8_t length;
                if (profile_id == kOneByteExtensionProfiledId) {
                    id = buffer[extension_offset + extensions_size_] >> 4;
                    length = 1 + (buffer[extension_offset + extensions_size_] & 0xF);
                    if (id == kOneByteHeaderExtensionReservedId || (id == kPaddingId && length != 1)) {
                        break;
                    }
                }else {
                    id = buffer[extension_offset + extensions_size_];
                    length = buffer[extension_offset + extensions_size_ + 1];
                }

                if (extensions_size_ + extension_header_length + length > extension_capacity) {
                    PLOG_WARNING << "Oversized RTP header extension.";
                    break;
                }

                ExtensionInfo& extension_info = FindOrCreateExtensionInfo(id);
                if (extension_info.length != 0) {
                    PLOG_VERBOSE << "Duplicate RTP header extension id: " << id << ", Overwriting.";
                }

                size_t offset = extension_offset + extensions_size_ + extension_header_length;
                if (!utils::numeric::is_value_in_range<uint16_t>(offset)) {
                    PLOG_WARNING << "Oversized RTP header extension.";
                    break;
                }
                extension_info.offset = static_cast<uint16_t>(offset);
                extension_info.length = length;
                extensions_size_ += extension_header_length + length;
            }
        }
        payload_offset_ = extension_offset + extension_capacity;
    }

    if (has_padding && payload_offset_ < size) {
        padding_size_ = buffer[size - 1];
        if (padding_size_ == 0) {
            PLOG_WARNING << "Padding was set, but padding size is zero.";
            return false;
        }
    }else {
        padding_size_ = 0;
    }

    if (payload_offset_ + padding_size_ > size) {
        return false;
    }

    payload_size_ = size - payload_offset_ - padding_size_;
    return true;
}

RtpPacket::ExtensionInfo& RtpPacket::FindOrCreateExtensionInfo(int id) {
    for (auto& extension : extension_entries_) {
        if (extension.id == id) {
            return extension;
        }
    }
    extension_entries_.emplace_back(id);
    return extension_entries_.back();
}

} // namespace naivertc
