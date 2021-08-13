#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_DLRR_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_DLRR_H_

#include "base/defines.hpp"

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace naivertc {
namespace rtcp {

struct RTC_CPP_EXPORT ReceiveTimeInfo {
    // RFC 3611 4.5
    ReceiveTimeInfo() : ssrc(0), last_rr(0), delay_since_last_rr(0) {}
    ReceiveTimeInfo(uint32_t ssrc, uint32_t last_rr, uint32_t delay)
        : ssrc(ssrc), last_rr(last_rr), delay_since_last_rr(delay) {}
    uint32_t ssrc;
    uint32_t last_rr;
    uint32_t delay_since_last_rr;
};

// DLRR Report Block: Delay since the Last Receiver Report (RFC 3611).
class RTC_CPP_EXPORT Dlrr {
public:
    static const uint8_t kBlockType = 5;
public:
    Dlrr();
    Dlrr(const Dlrr& other);
    ~Dlrr();

    Dlrr& operator=(const Dlrr& other) = default;

    // Dlrr without items treated same as no dlrr block.
    explicit operator bool() const { return !sub_blocks_.empty(); }

    // Second parameter is value read from block header,
    // i.e. size of block in 32bits excluding block header itself.
    bool Parse(const uint8_t* buffer, uint16_t block_length_32bits);

    size_t BlockSize() const;
    // Fills buffer with the Dlrr.
    // Consumes BlockLength() bytes.
    void PackInto(uint8_t* buffer, size_t size) const;

    void ClearItems() { sub_blocks_.clear(); }
    void AddDlrrItem(const ReceiveTimeInfo& time_info) {
        sub_blocks_.push_back(time_info);
    }

    const std::vector<ReceiveTimeInfo>& sub_blocks() const { return sub_blocks_; }

private:
    static const size_t kBlockHeaderSize = 4;
    static const size_t kSubBlockSize = 12;

    std::vector<ReceiveTimeInfo> sub_blocks_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif