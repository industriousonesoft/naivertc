#include "rtc/rtp_rtcp/rtcp/packets/target_bitrate.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

namespace naivertc {
namespace rtcp {
namespace {

constexpr size_t kBitrateItemSize = 4;

} // namespace

// BitrateItem
TargetBitrate::BitrateItem::BitrateItem() 
    : spatial_layer(0),
      temporal_layer(0),
      target_bitrate_kbps(0) {}
        
TargetBitrate::BitrateItem::BitrateItem(uint8_t spatial_layer,
                                        uint8_t temporal_layer,
                                        uint32_t target_bitrate_kbps) 
    : spatial_layer(spatial_layer),
      temporal_layer(temporal_layer),
      target_bitrate_kbps(target_bitrate_kbps) {}

//  RFC 4585: Feedback format.
//
//  Common packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |     BT=42     |   reserved    |         block length          |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
//  Target bitrate item (repeat as many times as necessary).
//
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |   S   |   T   |                Target Bitrate                 |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :  ...                                                          :
//
//  Spatial Layer (S): 4 bits
//    Indicates which temporal layer this bitrate concerns.
//
//  Temporal Layer (T): 4 bits
//    Indicates which temporal layer this bitrate concerns.
//
//  Target Bitrate: 24 bits
//    The encoder target bitrate for this layer, in kbps.
//
//  As an example of how S and T are intended to be used, VP8 simulcast will
//  use a separate TargetBitrate message per stream, since they are transmitted
//  on separate SSRCs, with temporal layers grouped by stream.
//  If VP9 SVC is used, there will be only one SSRC, so each spatial and
//  temporal layer combo used shall be specified in the TargetBitrate packet.

TargetBitrate::TargetBitrate() {}
    
TargetBitrate::~TargetBitrate() {}

const std::vector<TargetBitrate::BitrateItem>& TargetBitrate::GetTargetBitrates() const {
    return bitrates_;
}

void TargetBitrate::AddTargetBitrate(uint8_t spatial_layer,
                                     uint8_t temporal_layer,
                                     uint32_t target_bitrate_kbps) {
    assert(spatial_layer <= 0x0F);
    assert(temporal_layer <= 0x0F);
    assert(target_bitrate_kbps <= 0x00FFFFFF);
    bitrates_.push_back(BitrateItem(spatial_layer, temporal_layer, target_bitrate_kbps));
}

size_t TargetBitrate::BlockSize() const {
    return kBlockHeaderSize + bitrates_.size() * kBitrateItemSize;
}

bool TargetBitrate::Parse(const uint8_t* buffer, size_t size) {
    if (buffer[0] != kBlockType || size < kBlockHeaderSize) {
        return false;
    }
    // uint8_t reserved = buffer[1];
    uint16_t item_count = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
    size_t block_size = item_count * 4 + kBlockHeaderSize;
    if (size < block_size) {
        return false;
    }
    bitrates_.clear();
    size_t offset = kBlockHeaderSize;
    for (size_t i = 0; i < item_count; ++i) {
        uint8_t layers = buffer[offset];
        uint32_t bitrate_kbps = ByteReader<uint32_t, 3>::ReadBigEndian(&buffer[offset + 1]);
        offset += kBitrateItemSize;
        AddTargetBitrate((layers >> 4) & 0x0F, layers & 0x0F, bitrate_kbps);
    }
    return true;
}

void TargetBitrate::PackInto(uint8_t* buffer, size_t size) const {
    if (size < BlockSize()) {
        return;
    }
    buffer[0] = kBlockType;
    // Reserved
    buffer[1] = 0;
    uint16_t item_count_words = static_cast<uint16_t>(BlockSize() / 4 - 1);
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], item_count_words);

    size_t offset = kBlockHeaderSize;
    for (const auto& item : bitrates_) {
        buffer[offset] = (item.spatial_layer << 4) | item.temporal_layer;
        ByteWriter<uint32_t, 3>::WriteBigEndian(&buffer[offset + 1], item.target_bitrate_kbps);
        offset += kBitrateItemSize;
    }
}

} // namespace rtcp
} // namespace naivertc