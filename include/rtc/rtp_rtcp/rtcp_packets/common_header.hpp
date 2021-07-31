#ifndef _RTC_RTCP_COMMON_HEADER_H_
#define _RTC_RTCP_COMMON_HEADER_H_

#include "base/defines.hpp"

namespace naivertc {
namespace rtcp {

class RTC_CPP_EXPORT CommonHeader {
public:
    static constexpr size_t kFixedHeaderSize = 4;

    CommonHeader() {};
    CommonHeader(const CommonHeader&) = default;
    CommonHeader& operator=(const CommonHeader&) = default;
    ~CommonHeader() = default;

    uint8_t type() const { return packet_type_; }
    // Depending on packet type same header field can be used either as count or as feedback message type.
    // Caller expected to know how it is used
    uint8_t feedback_message_type() const { return count_or_fmt_; }
    uint8_t count() const { return count_or_fmt_; }
    uint32_t payload_size() const { return payload_size_; }
    const uint8_t* payload() const { return payload_; }
    size_t packet_size() const {
        return kFixedHeaderSize + payload_size_ + padding_size_;
    }

    // Returns pointer to the next RTCP packet in compound packet
    const uint8_t* NextPacket() const {
        return payload_ + payload_size_ + padding_size_;
    }

    bool ParseFrom(const uint8_t* buffer, size_t size);

private:
    uint8_t packet_type_ = 0;
    uint8_t count_or_fmt_ = 0;
    uint8_t padding_size_ = 0;
    uint32_t payload_size_ = 0;
    const uint8_t* payload_ = nullptr;
};
    
} // namespace rtcp
} // namespace naivertc

#endif