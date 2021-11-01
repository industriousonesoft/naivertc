#ifndef _RTC_CALL_RTP_UTILS_H_
#define _RTC_CALL_RTP_UTILS_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"

namespace naivertc {

bool IsRtcpPacket(ArrayView<const uint8_t> packet);
bool IsRtpPacket(ArrayView<const uint8_t> packet);

} // namespace naivertc

#endif