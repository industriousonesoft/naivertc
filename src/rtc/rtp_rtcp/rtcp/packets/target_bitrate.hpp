#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_TARGET_BITRATE_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_TARGET_BITRATE_H_

#include "base/defines.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {

class TargetBitrate {
public:
    static const uint8_t kBlockType = 42;

    struct BitrateItem {
        BitrateItem();
        BitrateItem(uint8_t spatial_layer,
                    uint8_t temporal_layer,
                    uint32_t target_bitrate_kbps);

        uint8_t spatial_layer;
        uint8_t temporal_layer;
        uint32_t target_bitrate_kbps;
    };
public:
    TargetBitrate();
    ~TargetBitrate();

    const std::vector<BitrateItem>& GetTargetBitrates() const;
    void AddTargetBitrate(uint8_t spatial_layer,
                          uint8_t temporal_layer,
                          uint32_t target_bitrate_kbps);

    size_t BlockSize() const;

    bool Parse(const uint8_t* buffer, size_t size);

    void PackInto(uint8_t* buffer, size_t size) const;

private:
    static const size_t kBlockHeaderSize = 4;
    
    std::vector<BitrateItem> bitrates_;
};

} // namespace rtcp
} // namespace naivertc

#endif