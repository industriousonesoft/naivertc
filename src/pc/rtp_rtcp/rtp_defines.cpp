#include "pc/rtp_rtcp/rtp_defines.hpp"

#include <sstream>
#include <cmath>

namespace naivertc {
    
// RTP
uint8_t RTP::version() const {
    return first_byte_ >> 6;
}

bool RTP::padding() const {
    return (first_byte_ >> 5) & 0x01;
}

bool RTP::extension() const {
    return (first_byte_ >> 4) & 0x01;
}

uint8_t RTP::csrc_count() const {
    return first_byte_ & 0x0F;
}

bool RTP::marker() const {
    return (payload_type_ >> 7) & 0x01;
}

uint8_t RTP::payload_type() const {
    // end with 'u' means unsigned
    return payload_type_ & 0b01111111u;
}

uint16_t RTP::seq_number() const {
    return ntohs(seq_number_);
}

uint32_t RTP::timestamp() const {
    return ntohl(timestamp_);
}

SSRC RTP::ssrc() const {
    return ntohl(ssrc_);
}

size_t RTP::header_size() const {
    return reinterpret_cast<const char*>(&csrc_) - reinterpret_cast<const char*>(this) + sizeof(CSRC) * csrc_count();
}

const char* RTP::payload_data() const {
    return reinterpret_cast<const char*>(&csrc_) - sizeof(CSRC) * csrc_count();
}

char* RTP::payload_data() {
    return reinterpret_cast<char*>(&csrc_) - sizeof(CSRC) * csrc_count();
}

RTP::operator std::string() const {
    std::ostringstream oss;
    oss << "RTP V: " << (int)version() << " P: " << (padding() ? "P" : " ")
        << " X: " << (extension() ? "X" : " ") << " CC: " << (int)csrc_count()
        << " M: " << (marker() ? "M" : " ") << " PT: " << (int)payload_type()
        << " SEQNO: " << seq_number() << " TS: " << timestamp();
    return oss.str();
}

void RTP::prepare() {
    // Reset
    first_byte_ = 0x00;
    // RTP version: high 2 bits, always 2
    first_byte_ |= (1 << 7);
    // TODO: to init left bits int first byte.
}

void RTP::set_seq_number(uint16_t new_seq_num) {
    seq_number_ = htons(new_seq_num);
}

void RTP::set_payload_type(uint8_t new_payload_type) {
    payload_type_ = (payload_type_ & 0b10000000u) /* marker: 1 bit */ |  (new_payload_type & 0b01111111u) /* payload type: 7 bits */;
}

void RTP::set_ssrc(SSRC in_ssrc) {
    ssrc_ = htonl(in_ssrc);
}

void RTP::set_marker(bool marker) {
    payload_type_ = (payload_type_ & 0x7F) | (marker ? 0x80 : 0x00);
}

void RTP::set_timestamp(uint32_t new_timestamp) {
    timestamp_ = htonl(new_timestamp);
}

} // namespace naivertc
