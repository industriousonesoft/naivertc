#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_DLRR_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_DLRR_H_

#include "base/defines.hpp"

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace naivertc {
namespace rtcp {

// DLRR Report Block: Delay since the Last Receiver Report (RFC 3611).
class Dlrr {
public:
    static const uint8_t kBlockType = 5;

    // RFC 3611 4.5
    struct TimeInfo {
        TimeInfo();
        TimeInfo(uint32_t ssrc, 
                 uint32_t last_rr, 
                 uint32_t delay);

        uint32_t ssrc;
        uint32_t last_rr;
        uint32_t delay_since_last_rr;
    };
public:
    Dlrr();
    Dlrr(const Dlrr& other);
    ~Dlrr();

    Dlrr& operator=(const Dlrr& other) = default;

    // Dlrr without items treated same as no dlrr block.
    explicit operator bool() const { return !time_infos_.empty(); }

    bool Parse(const uint8_t* buffer, size_t size);

    size_t BlockSize() const;
    // Fills buffer with the Dlrr.
    // Consumes BlockLength() bytes.
    void PackInto(uint8_t* buffer, size_t size) const;

    void Clear() { time_infos_.clear(); }
    void AddDlrrTimeInfo(const TimeInfo& info) {
        time_infos_.push_back(info);
    }

    const std::vector<TimeInfo>& time_infos() const { return time_infos_; }

private:
    static const size_t kBlockHeaderSize = 4;
    static const size_t kTimeInfoSize = 12;

    std::vector<TimeInfo> time_infos_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif