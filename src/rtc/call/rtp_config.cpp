#include "rtc/call/rtp_config.hpp"

namespace naivertc {
namespace {

std::optional<uint32_t> FindCorrespondingSsrc(const uint32_t target_ssrc, 
                                              const std::vector<uint32_t>& ssrcs, 
                                              const std::vector<uint32_t>& corresponding_ssrcs) {
    if (ssrcs.size() != corresponding_ssrcs.size()) {
        return std::nullopt;
    }
    for(size_t i = 0; i < ssrcs.size(); ++i) {
        if (target_ssrc == ssrcs[i]) {
            return corresponding_ssrcs[i];
        }
    }
    return std::nullopt;
}

} // namespace

// RtpConfig
RtpConfig::RtpConfig() = default;
RtpConfig::RtpConfig(const RtpConfig&) = default;
RtpConfig::~RtpConfig() = default;

bool RtpConfig::IsMediaSsrc(uint32_t ssrc) const {
    return std::find(media_ssrcs.begin(), media_ssrcs.end(), ssrc) != media_ssrcs.end();
}

bool RtpConfig::IsRtxSsrc(uint32_t ssrc) const {
    return std::find(rtx_ssrcs.begin(), rtx_ssrcs.end(), ssrc) != rtx_ssrcs.end();
}

bool RtpConfig::IsFlexfecSsrc(uint32_t ssrc) const {
    return flexfec.payload_type != -1 && ssrc == flexfec.ssrc;
}

std::optional<uint32_t> RtpConfig::RtxSsrcCorrespondToMediaSsrc(uint32_t media_ssrc) const {
    return FindCorrespondingSsrc(media_ssrc, media_ssrcs, rtx_ssrcs);
}

std::optional<uint32_t> RtpConfig::MediaSsrcCorrespondToRtxSsrc(uint32_t rtx_ssrc) const {
    return FindCorrespondingSsrc(rtx_ssrc, rtx_ssrcs, media_ssrcs);
}

// Ulpfec
RtpConfig::Ulpfec::Ulpfec() = default;
RtpConfig::Ulpfec::Ulpfec(const Ulpfec&) = default;
RtpConfig::Ulpfec::~Ulpfec() = default;

// Flexfec
RtpConfig::Flexfec::Flexfec() = default;
RtpConfig::Flexfec::Flexfec(const Flexfec&) = default;
RtpConfig::Flexfec::~Flexfec() = default;
    
} // namespace naivertc
