#include "rtc/sdp/sdp_media_entry_media.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace sdp {

std::string Media::ExtraMediaInfo() const {
    std::ostringstream desc;
    const std::string sp = " ";
    // RTP payload types
    for (const auto& kv : rtp_maps_) {
        desc << sp << kv.first;
    }
    return desc.str().substr(1 /* Trim the first space */);
}

std::string Media::GenerateSDPLines(const std::string eol) const {
    std::ostringstream oss;
    const std::string sp = " ";
    oss << MediaEntry::GenerateSDPLines(eol);

    // a=sendrecv
    oss << "a=" << direction_ << eol;

    // a=rtcp-mux
    if (rtcp_mux_enabled_) {
        oss << "a=rtcp-mux" << eol;
    }
    
    // a=rtcp-rsize
    if (rtcp_rsize_enabled_) {
        oss << "a=rtcp-rsize";
    }

    // a=extmap
    for (const auto& [id, map] : ext_maps_) {
        oss << "a=extmap:" << id << sp << map.uri;
    }

    // RTP maps
    for (const auto& [key, map] : rtp_maps_) {
        // a=rtpmap
        oss << "a=rtpmap:" << map.payload_type << sp << ToString(map.codec) << "/" << map.clock_rate;
        if (map.codec_params.has_value()) {
            oss << "/" << map.codec_params.value();
        }
        oss << eol;

        // a=rtcp-fb
        for (const auto& feebback : map.rtcp_feedbacks) {
            oss << "a=rtcp-fb:" << map.payload_type << sp << ToString(feebback) << eol;
        }

        // a=fmtp
        for (const auto& val : map.fmt_profiles) {
            oss << "a=fmtp:" << map.payload_type << sp << val << eol;
        }
    }

    // a=ssrc
    for (const auto& ssrc : media_ssrcs_) {
        std::optional<uint32_t> associated_rtx_ssrc = RtxSsrcAssociatedWithMediaSsrc(ssrc);
        std::optional<uint32_t> associated_fec_ssrc = FecSsrcAssociatedWithMediaSsrc(ssrc);
    
        // No associated ssrc
        if (!associated_rtx_ssrc && !associated_rtx_ssrc) {
            // a=ssrc
            // Media ssrc entry
            oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(ssrc), eol);
        } else {
            // a=ssrc-group:FID
            if (associated_rtx_ssrc) {
                oss << "a=ssrc-group:FID" << sp << ssrc << sp << associated_rtx_ssrc.value() << eol;
                // a=ssrc
                // Media ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(ssrc), eol);
                // RTX ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(associated_rtx_ssrc.value()), eol);

            }
            // a=ssrc-group:FEC
            if (associated_fec_ssrc) {
                oss << "a=ssrc-group:FEC" << sp << ssrc << sp << associated_rtx_ssrc.value() << eol;
                // a=ssrc
                // Media ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(ssrc), eol);
                // FEC ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(associated_fec_ssrc.value()), eol);
            }
        }
    }
    
    return oss.str();
}

std::string Media::GenerateSsrcEntrySDPLines(const SsrcEntry& entry, const std::string eol) const {
    std::ostringstream oss;
    const std::string sp = " ";
    if (entry.cname.has_value()) {
        oss << "a=ssrc:" << entry.ssrc << sp 
            << "cname:" << entry.cname.value() << eol;;
    } else {
        oss << "a=ssrc:" << entry.ssrc << eol;;
    }

    if (entry.msid.has_value()) {
        oss << "a=ssrc:" << entry.ssrc << sp 
            << "msid:" << entry.msid.value() << sp 
            << entry.track_id.value_or(entry.msid.value()) << eol;;
    }
    return oss.str();
}
    
} // namespace sdp
} // namespace naivertc
