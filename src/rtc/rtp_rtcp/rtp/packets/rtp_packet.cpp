#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"
#include "common/utils_numeric.hpp"

#include <plog/Log.h>

namespace naivertc {

namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kOneByteExtensionProfileId = 0xBEDE;
constexpr uint16_t kTwoByteExtensionProfileId = 0x1000;
constexpr uint16_t kTwoByteExtensionProfileIdAppBitsFilter = 0xFFF0;
constexpr size_t kOneByteExtensionHeaderSize = 1;
constexpr size_t kTwoByteExtensionHeaderSize = 2;
} // namespace

// RTP packet
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            Contributing source (CSRC) identifiers             |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |  header eXtension profile id  |       length in 32bits        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          Extensions                           |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                           Payload                             |
// |             ....              :  padding...                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               padding         | Padding size  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

RtpPacket::RtpPacket() 
    : RtpPacket(kIpPacketSize) {}

RtpPacket::RtpPacket(size_t capacity) 
    : RtpPacket(std::make_shared<ExtensionManager>(), capacity) {}

RtpPacket::RtpPacket(const RtpPacket&) = default;

RtpPacket::RtpPacket(std::shared_ptr<ExtensionManager> extension_manager) 
    : RtpPacket(std::move(extension_manager), kIpPacketSize) {}

RtpPacket::RtpPacket(std::shared_ptr<ExtensionManager> extension_manager, size_t capacity) 
    : Packet(capacity),
      extension_manager_(std::move(extension_manager)) {
    assert(capacity <= kIpPacketSize);
    Reset();
}

RtpPacket::~RtpPacket() {}

// Getter
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

// Setter
void RtpPacket::set_has_padding(bool has_padding) {
    has_padding_ = has_padding;
    if (has_padding) {
        WriteAt(0, data()[0] | 0x20);
    }else {
        WriteAt(0, data()[0] & ~0x20);
    }
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

bool RtpPacket::SetPadding(uint8_t padding_size) {
    if (payload_offset_ + payload_size_ + padding_size > capacity()) {
        PLOG_WARNING << "Cannot set padding size " << padding_size
                     << " , only"
                     << (capacity() - payload_offset_ - payload_size_)
                     << " bytes left in buffer.";
        return false;
    }
    padding_size_ = padding_size;
    resize(payload_offset_ + payload_size_ + padding_size_);
    if (padding_size_ > 0) {
        size_t padding_offset = payload_offset_ + payload_size_;
        size_t padding_end = padding_offset + padding_size_;
        memset(WriteAt(padding_offset), 0, padding_size_);
        WriteAt(padding_end - 1, padding_size_);
        // Reset padding bit
        set_has_padding(true);
    }else {
        // Clear padding bit
        set_has_padding(false);
    }
    return true;
}

void RtpPacket::SetPayload(ArrayView<const uint8_t> payload) {
    payload_size_ = payload.size();
    // Resize with new payload size
    BinaryBuffer::resize(payload_offset_ + payload_size_);
    // Insert payload at the front of the padding
    BinaryBuffer::insert(begin() + payload_offset_, payload.begin(), payload.end());
}

void RtpPacket::SetPayload(const uint8_t* buffer, size_t size) {
    auto raw_payload = ArrayView<const uint8_t>(buffer, size);
    SetPayload(std::move(raw_payload));
}

uint8_t* RtpPacket::SetPayloadSize(size_t size) {
    if (padding_size_ > 0) {
        PLOG_WARNING << "Failed to reset payload size when padding size is set.";
        return nullptr;
    }
    if (payload_offset_ + size > capacity()) {
        PLOG_WARNING << "Failed to reset payload size, not enough space in buffer.";
        return nullptr;
    }
    payload_size_ = size;
    BinaryBuffer::resize(payload_offset_ + payload_size_);
    return WriteAt(payload_offset_);
}

uint8_t* RtpPacket::AllocatePayload(size_t size) {
    SetPayloadSize(0);
    return SetPayloadSize(size);
}

// Write csrc list, Assumes:
// a) There is enough room left in buffer.
// b) Extension headers, payload or padding data has not already been added.
void RtpPacket::set_csrcs(ArrayView<const uint32_t> csrcs) {
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

void RtpPacket::Reset() {
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

bool RtpPacket::HasExtension(ExtensionType type) const {
    uint8_t id = extension_manager_->GetId(type);
    if (id == ExtensionManager::kInvalidId) {
        // Extension not registered
        return false;
    }
    return FindExtensionInfo(id) != nullptr;
}

bool RtpPacket::RemoveExtension(ExtensionType type) {
    // TODO: To remove extension by type
    return false;
}

// Parse
bool RtpPacket::Parse(const uint8_t* buffer, size_t size) {
    if (size > kIpPacketSize) {
        PLOG_WARNING << "Incoming buffer is too large for a RTP packet.";
        return false;
    }
    if (!ParseInternal(buffer, size)) {
        Reset();
        return false;
    }
    BinaryBuffer::resize(size);
    BinaryBuffer::assign(buffer, buffer + size);
    return true;
}

// Private methods
inline void RtpPacket::WriteAt(size_t offset, uint8_t byte) {
    BinaryBuffer::at(offset) = byte;
}

inline uint8_t* RtpPacket::WriteAt(size_t offset) {
    return &BinaryBuffer::at(offset);
}

inline const uint8_t* RtpPacket::WriteAt(size_t offset) const {
    return &BinaryBuffer::at(offset);
}

const RtpPacket::ExtensionInfo* RtpPacket::FindExtensionInfo(int id) const {
    for (const ExtensionInfo& extension : extension_entries_) {
        if (extension.id == id) {
            return &extension;
        }
    }
    return nullptr;
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

// Build
ArrayView<uint8_t> RtpPacket::AllocateExtension(ExtensionType type, size_t size) {
    if (size == 0 || size > ExtensionManager::kMaxValueSize ||
        (!extension_manager_->extmap_allow_mixed() &&
        size > ExtensionManager::kOneByteHeaderExtensionMaxValueSize)) {
        return nullptr;
    }

    uint8_t id = extension_manager_->GetId(type);
    if (id == ExtensionManager::kInvalidId) {
        // Extension not registered.
        return nullptr;
    }
    if (!extension_manager_->extmap_allow_mixed() &&
        id > ExtensionManager::kOneByteHeaderExtensionMaxId) {
        return nullptr;
    }
    return AllocateRawExtension(id, size);
}

ArrayView<uint8_t> RtpPacket::AllocateRawExtension(int id, size_t size) {
    if (id < ExtensionManager::kMinId || id > ExtensionManager::kMaxId) {
        return nullptr;
    }
    if (size < 1 || size > ExtensionManager::kMaxValueSize) {
        return nullptr;
    }
    const ExtensionInfo* extension_entry = FindExtensionInfo(id);
    // Extension already reserved. 
    if (extension_entry != nullptr) {
        // Check if same size is used.
        if (extension_entry->size == size) {
            return ArrayView<uint8_t>(WriteAt(extension_entry->offset), extension_entry->size);
        }else {
            PLOG_WARNING << "Length mismatch for extension id " << id
                         << ": expected "
                         << static_cast<int>(extension_entry->size)
                         << ". received " << size;
            return nullptr;
        }
    }

    if (payload_size_ > 0) {
        PLOG_WARNING << "Can't add new extension id " << id
                     << " after payload was set.";
        return nullptr;
    }
    if (padding_size_ > 0) {
        PLOG_WARNING << "Can't add new extension id " << id
                     << " after padding was set.";
        return nullptr;
    }

    const size_t num_csrc = data()[0] & 0x0F;
    const size_t extensions_offset = kFixedHeaderSize + (num_csrc * 4) + 4;

    // Determine if two-byte header is required for the extension based on id and
    // length. Please note that a length of 0 also requires two-byte header
    // extension. See RFC8285 Section 4.2-4.3.
    const bool two_byte_header_required = id > ExtensionManager::kOneByteHeaderExtensionMaxId ||
                                          size > ExtensionManager::kOneByteHeaderExtensionMaxValueSize ||
                                          size == 0;
    if (two_byte_header_required && !extension_manager_->extmap_allow_mixed()) {
        PLOG_WARNING << "Two bytes header required, but mixed extension is not allowed.";
        return nullptr;
    }

    uint16_t profile_id;
    if (extensions_size_ > 0) {
        profile_id = ByteReader<uint16_t>::ReadBigEndian(data() + extensions_offset - 4);
        if (profile_id == kOneByteExtensionProfileId && two_byte_header_required) {
            // Is buffer size big enough to fit promotion and new data field?
            // The header extension will grow with one byte per already allocated
            // extension + the size of the extension that is about to be allocated.
            size_t expected_new_extensions_size = extensions_size_ + extension_entries_.size() + kTwoByteExtensionHeaderSize + size;
            if (extensions_offset + expected_new_extensions_size > capacity()) {
                PLOG_WARNING
                    << "Extension cannot be registered: Not enough space left in "
                    "buffer to change to two-byte header extension and add new "
                    "extension.";
                return nullptr;
            }
            // Promote already written data to two-byte header format.
            PromoteToTwoByteHeaderExtension();
            profile_id = kTwoByteExtensionProfileId;
        }
    } else {
        // Profile specific ID, set to OneByteExtensionHeader unless
        // TwoByteExtensionHeader is required.
        profile_id = two_byte_header_required ? kTwoByteExtensionProfileId
                                            : kOneByteExtensionProfileId;
    }

    const size_t extension_header_size = profile_id == kOneByteExtensionProfileId
                                                    ? kOneByteExtensionHeaderSize
                                                    : kTwoByteExtensionHeaderSize;

    size_t new_extensions_size = extensions_size_ + extension_header_size + size;
    if (extensions_offset + new_extensions_size > capacity()) {
        PLOG_WARNING << "Extension cannot be registered: Not enough space left in buffer.";
        return nullptr;
    }

    // All checks passed, write down the extension headers.
    if (extensions_size_ == 0) {
        assert(payload_offset_ == kFixedHeaderSize + (num_csrc * 4));
        WriteAt(0, data()[0] | 0x10);  // Set extension bit.
        ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 4), profile_id);
    }

    if (profile_id == kOneByteExtensionProfileId) {
        uint8_t one_byte_header = utils::numeric::checked_static_cast<uint8_t>(id) << 4;
        one_byte_header |= utils::numeric::checked_static_cast<uint8_t>(size - 1);
        WriteAt(extensions_offset + extensions_size_, one_byte_header);
    } else {
        // TwoByteHeaderExtension.
        uint8_t extension_id = utils::numeric::checked_static_cast<uint8_t>(id);
        WriteAt(extensions_offset + extensions_size_, extension_id);
        uint8_t extension_length = utils::numeric::checked_static_cast<uint8_t>(size);
        WriteAt(extensions_offset + extensions_size_ + 1, extension_length);
    }

    const uint16_t extension_info_offset = utils::numeric::checked_static_cast<uint16_t>(extensions_offset + extensions_size_ + extension_header_size);
    const uint8_t extension_info_length = utils::numeric::checked_static_cast<uint8_t>(size);
    extension_entries_.emplace_back(id, extension_info_length, extension_info_offset);

    extensions_size_ = new_extensions_size;

    uint16_t extensions_size_padded = UpdateaExtensionSizeByAddZeroPadding(extensions_offset);
    payload_offset_ = extensions_offset + extensions_size_padded;
    BinaryBuffer::resize(payload_offset_);
    return ArrayView<uint8_t>(WriteAt(extension_info_offset), extension_info_length);
}

ArrayView<const uint8_t> RtpPacket::FindExtension(ExtensionType type) const {
    uint8_t id = extension_manager_->GetId(type);
    if (id == ExtensionManager::kInvalidId) {
        // Extension not registered.
        return nullptr;
    }
    ExtensionInfo const* extension_info = FindExtensionInfo(id);
    if (extension_info == nullptr) {
        return nullptr;
    }
    return ArrayView<const uint8_t>(WriteAt(extension_info->offset), extension_info->size);
}

uint16_t RtpPacket::UpdateaExtensionSizeByAddZeroPadding(size_t extensions_offset) {
    // Wrap extension size up to 32bit.
    uint16_t extensions_words = utils::numeric::checked_static_cast<uint16_t>((extensions_size_ + 3) / 4); 
    // Update header length field.
    ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 2), extensions_words);
    // Fill extension padding place with zeroes.
    size_t extension_padding_size = 4 * extensions_words - extensions_size_;
    memset(WriteAt(extensions_offset + extensions_size_), 0, extension_padding_size);
    return 4 * extensions_words;
}

void RtpPacket::PromoteToTwoByteHeaderExtension() {
    size_t num_csrc = data()[0] & 0x0F;
    size_t extensions_offset = kFixedHeaderSize + (num_csrc * 4) + 4;

    if(extension_entries_.size() == 0){
        return;
    }
    if(payload_size_ > 0) {
        PLOG_WARNING << "Can't resize extension fields after payload was set.";
        return;
    }
    // Not one-byte header extensions
    if(kOneByteExtensionProfileId != ByteReader<uint16_t>::ReadBigEndian(data() + extensions_offset - 4)) {
        return;
    }
    // Rewrite data.
    // Each extension adds one to the offset. The write-read delta for the last
    // extension is therefore the same as the number of extension entries.
    size_t write_read_delta = extension_entries_.size();
    for (auto extension_entry = extension_entries_.rbegin();
        extension_entry != extension_entries_.rend(); ++extension_entry) {
        size_t read_index = extension_entry->offset;
        size_t write_index = read_index + write_read_delta;
        // Update offset.
        extension_entry->offset = utils::numeric::checked_static_cast<uint16_t>(write_index);
        // Copy data. Use memmove since read/write regions may overlap.
        memmove(WriteAt(write_index), data() + read_index, extension_entry->size);
        // Rewrite id and length.
        WriteAt(--write_index, extension_entry->size);
        WriteAt(--write_index, extension_entry->id);
        --write_read_delta;
    }

    // Update profile header, extensions length, and zero padding.
    ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 4), kTwoByteExtensionProfileId);

    extensions_size_ += extension_entries_.size();
    uint16_t extensions_size_padded = UpdateaExtensionSizeByAddZeroPadding(extensions_offset);
    payload_offset_ = extensions_offset + extensions_size_padded;
    BinaryBuffer::resize(payload_offset_);
}

// Parse
bool RtpPacket::ParseInternal(const uint8_t* buffer, size_t size) {
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
    has_padding_ = has_padding;
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
       if (profile_id != kOneByteExtensionProfileId &&
            (profile_id & kTwoByteExtensionProfileIdAppBitsFilter) != kTwoByteExtensionProfileId) {
            PLOG_WARNING << "Unsupported RTP extension: " << profile_id;
        }else {
            size_t extension_header_length = profile_id == kOneByteExtensionProfileId ? kOneByteExtensionHeaderSize : kTwoByteExtensionHeaderSize;
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
                // One-Byte Header
                //    0                   1                   2                   3
                //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //   |       0xBE    |    0xDE       |           length=3            |
                //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //   |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
                //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //         ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
                //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //   |                          data                                 |
                //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                if (profile_id == kOneByteExtensionProfileId) {
                    id = buffer[extension_offset + extensions_size_] >> 4;
                    length = 1 + (buffer[extension_offset + extensions_size_] & 0xF);
                    if (id == kOneByteHeaderExtensionReservedId || (id == kPaddingId && length != 1)) {
                        break;
                    }
                }
                // Two-Byte Header
                // 0                   1                   2                   3
                // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |       0x10    |    0x00       |           length=3            |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |      ID       |     L=0       |     ID        |     L=1       |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |       data    |    0 (pad)    |       ID      |      L=4      |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |                          data                                 |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                else {
                    id = buffer[extension_offset + extensions_size_];
                    length = buffer[extension_offset + extensions_size_ + 1];
                }

                if (extensions_size_ + extension_header_length + length > extension_capacity) {
                    PLOG_WARNING << "Oversized RTP header extension.";
                    break;
                }

                size_t offset = extension_offset + extensions_size_ + extension_header_length;
                if (!utils::numeric::is_value_in_range<uint16_t>(offset)) {
                    PLOG_WARNING << "Oversized RTP header extension.";
                    break;
                }

                ExtensionInfo& extension_info = FindOrCreateExtensionInfo(id);
                if (extension_info.size != 0) {
                    PLOG_VERBOSE << "Duplicate RTP header extension id: " << id << ", Overwriting.";
                }

                extension_info.offset = static_cast<uint16_t>(offset);
                extension_info.size = length;
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

} // namespace naivertc
