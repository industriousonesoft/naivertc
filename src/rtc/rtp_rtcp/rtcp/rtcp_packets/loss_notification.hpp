#ifndef _RTC_RTP_RTCP_RTCP_PACKERTS_LOSS_NOTIFICATION_H_
#define _RTC_RTP_RTCP_RTCP_PACKERTS_LOSS_NOTIFICATION_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/psfb.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

namespace naivertc {
namespace rtcp {

class RTC_CPP_EXPORT LossNotification : public Psfb {
public:
  LossNotification();
  LossNotification(uint16_t last_decoded,
                   uint16_t last_received,
                   bool decodability_flag);
  LossNotification(const LossNotification& other);
  ~LossNotification() override;

  size_t PacketSize() const override;

  [[nodiscard]]
  bool PackInto(uint8_t* packet,
                size_t* index,
                size_t max_length,
                PacketReadyCallback callback) const override;

  // Parse assumes header is already parsed and validated.
  [[nodiscard]]
  bool Parse(const CommonHeader& packet);

  // Set all of the values transmitted by the loss notification message.
  // If the values may not be represented by a loss notification message,
  // false is returned, and no change is made to the object; this happens
  // when |last_recieved| is ahead of |last_decoded| by more than 0x7fff.
  // This is because |last_recieved| is represented on the wire as a delta,
  // and only 15 bits are available for that delta.
  [[nodiscard]]
  bool Set(uint16_t last_decoded,
           uint16_t last_received,
           bool decodability_flag);

  // RTP sequence number of the first packet belong to the last decoded
  // non-discardable frame.
  uint16_t last_decoded() const { return last_decoded_; }

  // RTP sequence number of the last received packet.
  uint16_t last_received() const { return last_received_; }

  // A decodability flag, whose specific meaning depends on the last-received
  // RTP sequence number. The decodability flag is true if and only if all of
  // the frame's dependencies are known to be decodable, and the frame itself
  // is not yet known to be unassemblable.
  // * Clarification #1: In a multi-packet frame, the first packet's
  //   dependencies are known, but it is not yet known whether all parts
  //   of the current frame will be received.
  // * Clarification #2: In a multi-packet frame, the dependencies would be
  //   unknown if the first packet was not received. Then, the packet will
  //   be known-unassemblable.
  bool decodability_flag() const { return decodability_flag_; }

private:
    static constexpr uint32_t kUniqueIdentifier = 0x4C4E5446;  // 'L' 'N' 'T' 'F'.
    static constexpr size_t kLossNotificationPayloadSize = 8;

    uint16_t last_decoded_;
    uint16_t last_received_;
    bool decodability_flag_;
};
        
} // namespace rtcp
} // namespace naivert 

#endif