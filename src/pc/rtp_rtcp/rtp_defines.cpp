#include "pc/rtp_rtcp/rtp_defines.hpp"

#include <sstream>
#include <cmath>

#ifndef htonll
#define htonll(x)                                                                                  \
	((uint64_t)(((uint64_t)htonl((uint32_t)(x))) << 32) | (uint64_t)htonl((uint32_t)((x) >> 32)))
#endif

#ifndef ntohll
#define ntohll(x) htonll(x)
#endif

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
    // version 2, no padding
    first_byte_ = 0b10000000u;
    // TODO: Need to init left bits in first byte?
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

// RTCP
// Header
uint8_t RTCP_Header::version() const {
    return first_byte_ >> 6;
}

bool RTCP_Header::padding() const {
    return (first_byte_ >> 5) & 0x01;
}

uint8_t RTCP_Header::report_count() const {
    return (first_byte_ & 0x0F);
}

uint8_t RTCP_Header::payload_type() const {
    return payload_type_;
}

uint16_t RTCP_Header::length() const {
    return ntohs(length_);
}

size_t RTCP_Header::length_in_bytes() const {
    // The length of this RTCP packet in 32 words minus one,
    // including the header and any padding, 
    // The offset of one makes zero a valid length and avoids
    // a possible infinite loop in scanning a RTCP packet, while
    // counting 32 bit words a validity check for a multiple of 4
    // 一个字节为8比特，而length的计算是以32比特为单位，因此换算成字节需要乘以4
    return (1 + length()) * 4;
}

void RTCP_Header::Prepare(uint8_t payload_type, uint8_t report_count, uint16_t length) {
    first_byte_ = 0b10000000u;
    set_report_count(report_count);
    set_payload_type(payload_type);
    set_length(length);
}

void RTCP_Header::set_payload_type(uint8_t type) {
    payload_type_ = type;
}

void RTCP_Header::set_report_count(uint8_t count) {
    first_byte_ = (first_byte_ & 0b11100000u) | (count & 0b00011111u);
}

void RTCP_Header::set_length(uint16_t length) {
    length_ = htons(length);
}

// void RTCP_Header::set_length_in_bytes(size_t length_in_bytes) {
//     length_ = (length_in_bytes - 1) / 4;
// }

RTCP_Header::operator std::string() const {
    std::ostringstream oss;
    oss << "RTCP Header: "
        << "V: " << version() << " P: " << (padding() ? "P" : " ")
        << "RC: " << report_count() << " PT: " << payload_type() 
        << "length: " << length();
    return oss.str();
}

} // namespace naivertc
