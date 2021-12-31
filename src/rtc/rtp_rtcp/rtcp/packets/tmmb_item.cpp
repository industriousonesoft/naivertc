#include "rtc/rtp_rtcp/rtcp/packets/tmmb_item.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 0 |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 4 | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TmmbItem::TmmbItem(uint32_t ssrc, uint64_t bitrate_bps, uint16_t overhead)
    : ssrc_(ssrc), bitrate_bps_(bitrate_bps), packet_overhead_(overhead) {
    assert(overhead <= 0x1ffu);
}

bool TmmbItem::Parse(const uint8_t* buffer) {
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[0]);
  // Read 4 bytes into 1 block.
  uint32_t compact = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
  // Split 1 block into 3 components.
  uint8_t exponent = compact >> 26;              // 6 bits.
  uint64_t mantissa = (compact >> 9) & 0x1ffff;  // 17 bits.
  uint16_t overhead = compact & 0x1ff;           // 9 bits.
  // Combine 3 components into 2 values.
  bitrate_bps_ = (mantissa << exponent);

  bool shift_overflow = (bitrate_bps_ >> exponent) != mantissa;
  if (shift_overflow) {
    PLOG_WARNING << "Invalid tmmb bitrate value : "
                 << mantissa 
                 << "*2^"
                 << static_cast<int>(exponent);
    return false;
  }
  packet_overhead_ = overhead;
  return true;
}

bool TmmbItem::PackInto(uint8_t* buffer, size_t size) const {
    if (size < kFixedTmmbItemSize) {
        return false;
    }
    constexpr uint64_t kMaxMantissa = 0x1ffff;  // 17 bits.
    uint64_t mantissa = bitrate_bps_;
    uint32_t exponent = 0;
    while (mantissa > kMaxMantissa) {
        mantissa >>= 1;
        ++exponent;
    }

    ByteWriter<uint32_t>::WriteBigEndian(&buffer[0], ssrc_);
    uint32_t compact = (exponent << 26) | (mantissa << 9) | packet_overhead_;
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[4], compact);
    return true;
}

void TmmbItem::set_packet_overhead(uint16_t overhead) {
    assert(overhead <= 0x1ffu);
    packet_overhead_ = overhead;
}
    
} // namespace rtcp
} // namespace naivertc
