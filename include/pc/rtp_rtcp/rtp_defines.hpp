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



#pragma pack(pop)

} // namespace naivertc


#endif