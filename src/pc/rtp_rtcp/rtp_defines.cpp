#include "pc/rtp_rtcp/rtp_defines.hpp"

#include <sstream>
#include <cmath>

#warning 参考WebRTC modules/rtp_rtcp
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

// SR: Send Report
// Report Block
uint16_t RTCP_ReportBlock::seq_num_cycles() const {
    return ntohs(seq_num_cycles_);
}

uint16_t RTCP_ReportBlock::highest_seq_num() const {
    return ntohs(highest_seq_num_);
}

uint32_t RTCP_ReportBlock::jitter() const {
    return ntohl(jitter_);
}

uint32_t RTCP_ReportBlock::delay_since_last_sr() const {
    return ntohl(delay_since_last_sr_);
}

SSRC RTCP_ReportBlock::ssrc() const {
    return ntohl(ssrc_);
}

uint32_t RTCP_ReportBlock::last_sr_ntp_timestamp() const {
    // FIXME: 此处需要：ntohl(last_sr_ntp_timestamp_) << 16u？
    // 我觉得是不需要， 在设置的时候uint64_t强制转换为uint32_t，因此高位自动被裁减了
    return ntohl(last_sr_ntp_timestamp_);
}

RTCP_ReportBlock::operator std::string() const {
    std::ostringstream oss;
    oss << "RTCP report block: "
        << "ssrc="
        << ntohl(ssrc_)
        // TODO: Implement these reports
        //	<< ", fractionLost=" << fractionLost
        //	<< ", packetsLost=" << packetsLost
        << ", highest_seq_num=" << highest_seq_num() << ", seq_num_cycles=" << seq_num_cycles()
        << ", jitter=" << jitter() << ", last_sr_ntp_timestamp=" << last_sr_ntp_timestamp()
        << ", delay_since_last_sr=" << delay_since_last_sr();

    return oss.str();
}

void RTCP_ReportBlock::set_ssrc(SSRC ssrc) {
    ssrc_ = htonl(ssrc);
}

void RTCP_ReportBlock::set_packet_lost(unsigned int packet_lost, unsigned int total_packets) {
    // TODO: Calculate loss percentage
    // See https://datatracker.ietf.org/doc/html/rfc3550#section-6.4.1
    fracion_lost_and_packet_lost_ = 0;
}

void RTCP_ReportBlock::set_seq_num(uint16_t highest_seq_num, uint16_t seq_num_cycles) {
    highest_seq_num_ = htons(highest_seq_num);
    seq_num_cycles_ = htons(seq_num_cycles);
}   

void RTCP_ReportBlock::set_jitter(uint32_t jitter) {
    jitter_ = htonl(jitter);
}

void RTCP_ReportBlock::set_last_sr_ntp_timestamp(uint64_t ntp_timestamp) {
    // Keep the middle 32 bits out of 64 in the NTP timestamp
    last_sr_ntp_timestamp_ = htonll(ntp_timestamp >> 16u);
}

void RTCP_ReportBlock::set_delay_since_last_sr(uint32_t delay) {
    delay_since_last_sr_ = htonl(delay);
}

void RTCP_ReportBlock::Prepare(SSRC ssrc, unsigned int packet_lost, unsigned int total_packets, 
                                uint16_t highest_seq_num, uint16_t seq_num_cycles, 
                                uint32_t jitter, uint64_t last_sr_ntp_timestamp, uint64_t delay_since_last_sr) {
    set_ssrc(ssrc);
    set_packet_lost(packet_lost, total_packets);
    set_seq_num(highest_seq_num, seq_num_cycles);
    set_jitter(jitter);
    set_last_sr_ntp_timestamp(last_sr_ntp_timestamp);
    set_delay_since_last_sr(delay_since_last_sr);
}

unsigned int RTCP_ReportBlock::LossPercentage() const {
    // TODO: Calculate loss percentage
    return 0;
}

unsigned int RTCP_ReportBlock::PacketLostCount() const {
    // TODO: Calculate total percentage
    return 0;
}


} // namespace naivertc
