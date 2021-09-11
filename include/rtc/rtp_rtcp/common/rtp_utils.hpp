#ifndef _RTC_RTP_RTCP_COMMON_RTP_UTILS_H_
#define _RTC_RTP_RTCP_COMMON_RTP_UTILS_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"

namespace naivertc {
namespace rtp {
namespace utils {

bool IsRtcpPacket(ArrayView<const uint8_t> packet);
bool IsRtpPacket(ArrayView<const uint8_t> packet);

} // namespace utils
} // namespace rtp
} // namespace naivertc


#endif