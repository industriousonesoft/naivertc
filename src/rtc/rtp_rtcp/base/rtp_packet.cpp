#include "rtc/rtp_rtcp/base/rtp_packet.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

namespace naivertc {

namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kDefaultPacketSize = 1500;
} // namespace

/*
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
    : Packet(){}
    
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

} // namespace naivertc
