#ifndef _RTC_RTCP_NACK_H_
#define _RTC_RTCP_NACK_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/psfb.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {
class CommonHeader;

// Negative acknowledgements, RFC 4585, section 6.2.1
class RTC_CPP_EXPORT Nack : public Psfb {
public:
    static constexpr uint8_t kFeedbackMessageType = 1;
public:
    Nack();
    Nack(const Nack&);
    ~Nack() override;

    const std::vector<uint16_t>& packet_ids() const { return packet_ids_; }
    void set_packet_ids(const uint16_t* nack_list, size_t size);
    void set_packet_ids(std::vector<uint16_t> nack_list);

    bool Parse(const CommonHeader& packet);

    size_t PacketSize() const override;
    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    static constexpr size_t kFciItemSize = 4;
    struct FciItem {
      uint16_t first_pid;
      uint16_t bitmask;  
    };

    void PackFciItems();
    void UnpackFciItems();

private:
    std::vector<FciItem> fci_items_;
    std::vector<uint16_t> packet_ids_;
};

} // namespace rtcp
} // namespace naivertc


#endif