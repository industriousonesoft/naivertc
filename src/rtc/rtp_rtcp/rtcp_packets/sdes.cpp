#include "rtc/rtp_rtcp/rtcp_packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// Source Description (SDES) (RFC 3550).
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// header |V=2|P|    SC   |  PT=SDES=202  |             length            |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_1                          |
//   1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_2                          |
//   2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// Canonical End-Point Identifier SDES Item (CNAME)
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |    CNAME=1    |     length    | user and domain name        ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

namespace {
constexpr uint8_t kTerminatorTag = 0;
constexpr size_t kTerminatorSize = 1;
constexpr uint8_t kCNameTag = 1;
constexpr size_t kChunkBaseSize = 8;
constexpr int kPaddingValue = 0;

size_t CalculateChunkSize(const Sdes::Chunk& chunk) {
    // Chunk:
    // SSRC/CSRC (4 bytes) | CNAME=1 (1 byte) | length (1 byte) | cname | padding.
    size_t chunk_payload_size = 4 + 1 + 1 + chunk.cname.size();
    // Padding size range [1, 4]
    size_t padding_size = 4 - (chunk_payload_size % 4);
    return chunk_payload_size + padding_size;
}

}

Sdes::Sdes() : packet_size_(RtcpPacket::kFixedRtcpCommonHeaderSize) {}

Sdes::~Sdes() {}

bool Sdes::AddCName(uint32_t ssrc, std::string cname) {
    if (cname.length() > 0xFFu /* One byte */) {
        PLOG_WARNING << "Max cname length reached.";
        return false;
    }
    if (chunks_.size() >= kMaxNumberOfChunks) {
        PLOG_WARNING << "Max SDES chunks reached.";
        return false;
    }
    Chunk chunk;
    chunk.ssrc = ssrc;
    chunk.cname = std::move(cname);
    packet_size_ += CalculateChunkSize(chunk);
    chunks_.push_back(std::move(chunk));
    return true;
}

bool Sdes::Parse(const CommonHeader& packet) {
    if (packet.type() != kPacketType) {
        PLOG_WARNING << "Incomming packet is not a SDES(Source Description) packet.";
        return false;
    }
    if (packet.payload_size() % 4 != 0) {
        PLOG_WARNING << "Invalid payload size "
                     << packet.payload_size()
                     << " bytes for a valid SDES packet. Size should be multiple of 4 bytes."; 
        return false;
    }
    uint8_t number_of_chunks = packet.count();
    size_t packet_size = kFixedRtcpCommonHeaderSize;
    const uint8_t* buffer = packet.payload();
    size_t buffer_size = packet.payload_size();
    size_t index = 0;
    std::vector<Chunk> chunks_tmp;
    chunks_tmp.resize(number_of_chunks);
    for (size_t i = 0; i < number_of_chunks;) {
        // Each chunk consumes at least 8 bytes.
        if (packet_size - index < kChunkBaseSize ) {
            PLOG_WARNING << "Not enough space left for chunk #" << (i + 1);
            return false;
        }
        chunks_tmp[i].ssrc = ByteReader<uint32_t>::ReadBigEndian(&buffer[index]);
        index += sizeof(uint32_t);
        bool cname_found = false;
        uint8_t item_type;
        while ((item_type = buffer[index++]) != kTerminatorTag) {
            if (index >= buffer_size) {
                PLOG_WARNING << "Unexpected end of packet while reading chunk #" << (i + 1)
                             << ". Expected to find length of the cname.";
                return false;
            }
            uint8_t item_length = buffer[index++];
            if (index + item_length + kTerminatorSize > buffer_size) {
                PLOG_WARNING << "Unexpected end of packet while reading chunk #" << (i + 1)
                             << ". Expected to find cname of length: " << item_length;
                return false;
            }

            if (item_type == kCNameTag) {
                if (cname_found) {
                    PLOG_WARNING << "Found extra CNAME for same ssrc in chunk #" << (i + 1);
                    return false;
                }
                cname_found = true;
                chunks_tmp[i].cname.assign(reinterpret_cast<const char*>(&buffer[index]), item_length);
            }
            index += item_length;
        } // end of while

        if (cname_found) {
            packet_size += CalculateChunkSize(chunks_tmp[i]);
            ++i;
        }else {
            // RFC stats CNAME item is mandatory, but same time it allows chunk without items.
            // So while parsing, ignore all chunks without cname, but do not fail the parse.
            PLOG_WARNING << "CNAME not found for ssrc " << chunks_tmp[i].ssrc;
            --number_of_chunks;
            chunks_tmp.resize(number_of_chunks);
        }
        // Add padding of current chunk to adjust to 23-bit boundary
        index += (buffer_size - index) % 4;
    } // end of for

    chunks_.swap(chunks_tmp);
    packet_size_ = packet_size;
    return true;
}

size_t Sdes::PacketSize() const {
    return packet_size_;
} 

bool Sdes::PackInto(uint8_t* buffer,
                    size_t* index,
                    size_t max_size,
                    PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }
    const size_t index_end = *index + PacketSize();
    RtcpPacket::CreateCommonHeader(chunks_.size(), kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);

    for (const auto& chunk : chunks_) {
        ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 0], chunk.ssrc);
        ByteWriter<uint8_t>::WriteBigEndian(&buffer[*index + 4], kCNameTag);
        ByteWriter<uint8_t>::WriteBigEndian(&buffer[*index + 5], static_cast<uint8_t>(chunk.cname.size()));
        memcpy(&buffer[*index + 6], chunk.cname.data(), chunk.cname.size());
        size_t chunk_size = 6 + chunk.cname.size();
        *index += chunk_size;

        size_t padding_size = 4 - (chunk_size % 4);
        memset(buffer + *index, kPaddingValue, padding_size);
        *index += padding_size;
    }

    assert(*index == index_end && "Unmatched end of index");
   
    return true;
}
    
} // namespace rtcp
} // namespace naivertc
