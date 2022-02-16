#ifndef _RTC_SDP_MEDIA_ENTRY_MEDIA_H_
#define _RTC_SDP_MEDIA_ENTRY_MEDIA_H_

#include "rtc/sdp/sdp_media_entry.hpp"

#include <unordered_map>
#include <functional>
#include <string>
#include <sstream>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Media : public MediaEntry {
public:
    enum class Codec {
        // Audio codec
        OPUS,
        // Video codecs
        VP8,
        VP9,
        H264,
        // Protection codecs
        RED,
        ULP_FEC,
        FLEX_FEC,
        RTX
    };

    enum class RtcpFeedback {
        NACK,
        GOOG_REMB,
        TRANSPORT_CC
    };

    // ExtMap
    struct ExtMap {
        int id;
        std::string uri;

        ExtMap(int id, std::string uri);
    };

    // RtpMap
    struct RtpMap {
        int payload_type;
        Codec codec;
        int clock_rate;
        std::optional<std::string> codec_params = std::nullopt;
        std::optional<int> rtx_payload_type = std::nullopt;
        std::vector<RtcpFeedback> rtcp_feedbacks;
        std::vector<std::string> fmt_profiles;
        
        RtpMap(int payload_type, 
               Codec codec, 
               int clock_rate, 
               std::optional<std::string> codec_params = std::nullopt,
               std::optional<int> rtx_payload_type = std::nullopt);
    };

    // SsrcEntry
    struct SsrcEntry {
        enum class Kind { MEDIA, RTX, FEC };
        uint32_t ssrc = 0;
        Kind kind = Kind::MEDIA;
        std::optional<std::string> cname = std::nullopt;
        std::optional<std::string> msid = std::nullopt;
        std::optional<std::string> track_id = std::nullopt;

        SsrcEntry(uint32_t ssrc,
                  Kind kind,
                  std::optional<std::string> cname = std::nullopt, 
                  std::optional<std::string> msid = std::nullopt, 
                  std::optional<std::string> track_id = std::nullopt);
    };

    static std::string ToString(Codec codec);
    static std::string ToString(RtcpFeedback rtcp_feedback);

public:
    Media(Kind kind, 
          std::string mid, 
          std::string protocols,
          Direction direction = Direction::INACTIVE);
    Media(const MediaEntry& entry, Direction direction);
    Media(MediaEntry&& entry, Direction direction);
    virtual ~Media();

    Direction direction() const { return direction_; };
    void set_direction(Direction direction) { direction_ = direction; };

    void set_bandwidth_max_value(int value) { bandwidth_max_value_ = value; };
    int bandwidth_max_value() const { return bandwidth_max_value_; };

    bool rtcp_mux_enabled() const { return rtcp_mux_enabled_; };
    void set_rtcp_mux_enabled(bool enabled) { rtcp_mux_enabled_ = enabled; };

    bool rtcp_rsize_enabled() const { return rtcp_rsize_enabled_; };
    void set_rtcp_rsize_enabled(bool enabled) { rtcp_rsize_enabled_ = enabled; };

    // Ssrc
    SsrcEntry* AddSsrc(uint32_t ssrc, 
                       SsrcEntry::Kind kind,
                       std::optional<std::string> cname = std::nullopt, 
                       std::optional<std::string> msid = std::nullopt, 
                       std::optional<std::string> track_id = std::nullopt);
    SsrcEntry* AddSsrc(SsrcEntry ssrc_entry);
    void RemoveSsrc(uint32_t ssrc);
    bool IsMediaSsrc(uint32_t ssrc) const;
    bool IsRtxSsrc(uint32_t ssrc) const;
    bool IsFecSsrc(uint32_t ssrc) const;
    std::vector<uint32_t> media_ssrcs() const { return media_ssrcs_; };
    // If we use RTX there MUST be an association media_ssrcs[i] <-> rtx_ssrcs[i].
    std::vector<uint32_t> rtx_ssrcs() const { return rtx_ssrcs_; };
    // If we use FEC there MUST be an association media_ssrcs[i] <-> fec_ssrcs[i].
    std::vector<uint32_t> fec_ssrcs() const { return fec_ssrcs_; };
    SsrcEntry* ssrc(uint32_t ssrc);
    const SsrcEntry* ssrc(uint32_t ssrc) const;
    SsrcEntry::Kind ssrc_kind(uint32_t ssrc) const;
    void ForEachSsrc(std::function<void(const SsrcEntry& ssrc_entry)>&& handler) const;
    void ClearSsrcs();

    std::optional<uint32_t> RtxSsrcAssociatedWithMediaSsrc(uint32_t ssrc) const;
    std::optional<uint32_t> FecSsrcAssociatedWithMediaSsrc(uint32_t ssrc) const;

    // Codec
    Media::RtpMap* AddCodec(int payload_type, 
                            Codec codec,
                            int clock_rate,
                            std::optional<const std::string> codec_params = std::nullopt,
                            std::optional<const std::string> profile = std::nullopt);
    
    // RtpMap
    bool AddFeedback(int payload_type, RtcpFeedback feed_back);
    RtpMap* AddRtpMap(RtpMap map);
    void ForEachRtpMap(std::function<void(const RtpMap& rtp_map)>&& handler) const;
    void ClearRtpMap();
    bool HasPayloadType(int pt) const;
    std::vector<int> PayloadTypes() const;

    // Rtp extension map
    ExtMap* AddExtMap(ExtMap ext_map);
    bool RemoveExtMap(int id);
    bool RemoveExtMap(std::string uri);
    void ClearExtMap();
    void ForEachExtMap(std::function<void(const ExtMap& ext_map)>&& handler) const;

    virtual bool ParseSDPLine(std::string_view line) override;
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;

    Media Reciprocated() const;

private:
    static std::optional<RtpMap> ParseRtpMap(const std::string_view& attr_value);
    std::string ExtraMediaInfo() const override;
    virtual std::string GenerateSDPLines(const std::string eol) const override;
    std::string GenerateSsrcEntrySDPLines(const SsrcEntry& entry, const std::string eol) const;
private:
    Direction direction_;
    
    // rtcp-mux: Rtp and Rtcp share the same socket and connection
    // instead of using two separate connections.
    bool rtcp_mux_enabled_;
    // rtcp-rsize: RTCP Reduced-Size mode
    bool rtcp_rsize_enabled_;
    std::unordered_map<int, ExtMap> ext_maps_;
    std::unordered_map<int, RtpMap> rtp_maps_;

    std::vector<uint32_t> media_ssrcs_;
    std::vector<uint32_t> rtx_ssrcs_;
    std::vector<uint32_t> fec_ssrcs_;
    std::unordered_map<uint32_t, SsrcEntry> ssrc_entries_;

    int bandwidth_max_value_ = -1;
};



} // namespace sdp
} // namespace naivert 

#endif