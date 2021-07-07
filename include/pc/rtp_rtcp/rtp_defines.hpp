#ifndef _PC_RTP_DEFINES_H_
#define _PC_RTP_DEFINES_H_

#include "base/defines.hpp"

#include <stddef.h>
#include <string>

namespace naivertc {
    
using SSRC = uint32_t;
using CSRC = uint32_t;

// 字节对齐为1，即不自动填充
#pragma pack(push, 1)

/** 
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
// RTP
struct RTC_CPP_EXPORT RTP {
private:
    // All Variales saved in big-endian
    uint8_t first_byte_;
    uint8_t payload_type_;
    uint16_t seq_number_;
    uint32_t timestamp_;
    SSRC ssrc_;
    CSRC csrc_[16];

public:
    uint8_t version() const;
    bool padding() const;
    bool extension() const;
    uint8_t csrc_count() const;
    bool marker() const;
    uint8_t payload_type() const;
    uint16_t seq_number() const;
    uint32_t timestamp() const;
    SSRC ssrc() const;

    size_t header_size() const;
    const char* payload_data() const;
    char* payload_data();

    operator std::string() const;

    void prepare();
    void set_seq_number(uint16_t seq_num);
    void set_payload_type(uint8_t type);
    void set_ssrc(SSRC ssrc);
    void set_marker(bool marker);
    void set_timestamp(uint32_t timestamp);
};

// RTCP
/** 
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|   RC    |      PT       |           length              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |                    source or chunk                            |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
// Header
struct RTC_CPP_EXPORT RTCP_Header {
private:
    // All Variales saved in big-endian
    uint8_t first_byte_;
    uint8_t payload_type_;
    uint16_t length_;

public:
    uint8_t version() const;
    bool padding() const;
    uint8_t report_count() const;
    uint8_t payload_type() const;
    uint16_t length() const;
    size_t length_in_bytes() const;

    void set_payload_type(uint8_t type);
    void set_report_count(uint8_t count);
    void set_length(uint16_t length);
    // void set_length_in_bytes(size_t length_in_bytes);

    operator std::string() const;

    void Prepare(uint8_t payload_type, uint8_t report_count, uint16_t length);
};

// SR: Send Report
// Report Block
/** 
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    |                 SSRC_1 (SSRC of first source)                 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | fraction lost |       cumulative number of packets lost       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           extended highest sequence number received           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      interarrival jitter                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         last SR (LSR)                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   delay since last SR (DLSR)                  |
    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
struct RTC_CPP_EXPORT RTCP_ReportBlock {
private:
    // All Variales saved in big-endian
    // ssrc
    SSRC ssrc_;
    // fraction lost is high 8 bits, packets lost is low 24 bits
    uint32_t fracion_lost_and_packet_lost_;
    uint16_t seq_num_cycles_;
    uint16_t highest_seq_num_;
    uint32_t jitter_;
    // Last send report timestamp: 
    // The middle 32 bits out of 64 in the NTP timestamp
    uint32_t last_sr_ntp_timestamp_;
    // Delay since last send report: 
    // The delay, expressed in units of 1/65536 seconds, between
    // receiving the last SR packet from source SSRC_n and sending this
    // reception report block. 
    uint32_t delay_since_last_sr_;

public:
    uint16_t seq_num_cycles() const;
    uint16_t highest_seq_num() const;
    uint32_t jitter() const;
    uint32_t delay_since_last_sr() const;

    SSRC ssrc() const;
    uint32_t last_sr_ntp_timestamp() const;

    operator std::string() const;

    void set_ssrc(SSRC ssrc);
    void set_packet_lost(unsigned int packet_lost, unsigned int total_packets);
    void set_seq_num(uint16_t highest_seq_num, uint16_t seq_num_cycles);
    void set_jitter(uint32_t jitter);
    void set_last_sr_ntp_timestamp(uint64_t ntp_timestamp);
    void set_delay_since_last_sr(uint32_t delay);

    void Prepare(SSRC ssrc, unsigned int packet_lost, unsigned int total_packets, 
                uint16_t highest_seq_num, uint16_t seq_num_cycles, 
                uint32_t jitter, uint64_t last_sr_ntp_timestamp, uint64_t delay_since_last_sr);

    unsigned int LossPercentage() const;
    unsigned int PacketLostCount() const;
};

// See: https://datatracker.ietf.org/doc/html/rfc3550#section-6.4.1
/** 
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         SSRC of sender                        |
    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    |              NTP timestamp, most significant word             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |             NTP timestamp, least significant word             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         RTP timestamp                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     sender's packet count                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      sender's octet count                     |
    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
struct RTC_CPP_EXPORT RTCP_SR {

};

#pragma pack(pop)

} // namespace naivertc


#endif