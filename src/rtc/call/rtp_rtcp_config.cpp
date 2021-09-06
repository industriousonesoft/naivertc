#include "rtc/call/rtp_rtcp_config.hpp"

namespace naivertc {

// RtpRtcpConfig
RtpRtcpConfig::RtpRtcpConfig() = default;
RtpRtcpConfig::RtpRtcpConfig(const RtpRtcpConfig&) = default;
RtpRtcpConfig::~RtpRtcpConfig() = default;

// Ulpfec
RtpRtcpConfig::Ulpfec::Ulpfec() = default;
RtpRtcpConfig::Ulpfec::Ulpfec(const Ulpfec&) = default;
RtpRtcpConfig::Ulpfec::~Ulpfec() = default;

// Flexfec
RtpRtcpConfig::Flexfec::Flexfec() = default;
RtpRtcpConfig::Flexfec::Flexfec(const Flexfec&) = default;
RtpRtcpConfig::Flexfec::~Flexfec() = default;
    
} // namespace naivertc
