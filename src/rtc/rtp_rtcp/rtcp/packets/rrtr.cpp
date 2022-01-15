#include "rtc/rtp_rtcp/rtcp/packets/rrtr.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

namespace naivertc {
namespace rtcp {
namespace {

constexpr size_t kBlockSize = 12;
    
} // namespace


// Receiver Reference Time Report Block (RFC 3611, section 4.4).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |     BT=4      |   reserved    |       block length = 2        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |              NTP timestamp, most significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |             NTP timestamp, least significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Rrtr::Rrtr() {}
    
Rrtr::~Rrtr() {}

size_t Rrtr::BlockSize() const {
    return kBlockSize;
}

NtpTime Rrtr::ntp() const {
    return ntp_;
}

void Rrtr::set_ntp(NtpTime ntp) {
    ntp_ = ntp;
}

bool Rrtr::Parse(const uint8_t* buffer, size_t size) {
    if (size < 12 || buffer[0] != kBlockType) {
        return false;
    }
    // uint8_t reserved = buffer[1];
    uint32_t seconds = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
    uint32_t fractions = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
    ntp_.Set(seconds, fractions);
    return true;
}

void Rrtr::PackInto(uint8_t* buffer, size_t size) const {
    if (size < 12) {
        return;
    }
    buffer[0] = kBlockType;
    buffer[1] = 0; // Reserved
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], 2);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[4], ntp_.seconds());
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[8], ntp_.fractions());
}
    
} // namespace rtcp    
} // namespace naivertc