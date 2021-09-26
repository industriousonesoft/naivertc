#ifndef _RTC_SDP_MEDIA_ENTRY_MEDIA_H_
#define _RTC_SDP_MEDIA_ENTRY_MEDIA_H_

#include "rtc/sdp/sdp_media_entry.hpp"

#include <map>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Media : public MediaEntry {
public:
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
public:
    Media(); // For Template in TaskQueue
    Media(Type type, 
          std::string mid, 
          const std::string protocols,
          Direction direction = Direction::SEND_ONLY);
    Media(const MediaEntry& entry, Direction direction);
    Media(MediaEntry&& entry, Direction direction);
    virtual ~Media();

    Direction direction() const { return direction_; };
    void set_direction(Direction direction) { direction_ = direction; };

    void set_bandwidth_max_value(int value) { bandwidth_max_value_ = value; };
    int bandwidth_max_value() const { return bandwidth_max_value_; };

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
    const std::vector<uint32_t> media_ssrcs() const { return media_ssrcs_; };
    // If we use RTX there MUST be an association media_ssrcs[i] <-> rtx_ssrcs[i].
    const std::vector<uint32_t> rtx_ssrcs() const { return rtx_ssrcs_; };
    // If we use FEC there MUST be an association media_ssrcs[i] <-> fec_ssrcs[i].
    const std::vector<uint32_t> fec_ssrcs() const { return fec_ssrcs_; };
    SsrcEntry* ssrc(uint32_t ssrc);
    const SsrcEntry* ssrc(uint32_t ssrc) const;
    SsrcEntry::Kind kind(uint32_t ssrc) const;
    void ClearAllSsrcs();

    std::optional<uint32_t> RtxSsrcAssociatedWithMediaSsrc(uint32_t ssrc) const;
    std::optional<uint32_t> FecSsrcAssociatedWithMediaSsrc(uint32_t ssrc) const;

    bool AddFeedback(int payload_type, const std::string feed_back);
    bool HasPayloadType(int pt) const;

    virtual bool ParseSDPLine(std::string_view line) override;
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;

    Media ReciprocatedSDP() const;

protected:
    struct RtpMap {
        int payload_type;
        std::string codec;
        int clock_rate;
        std::optional<std::string> codec_params = std::nullopt;
        std::vector<std::string> rtcp_feedbacks;
        std::vector<std::string> fmt_profiles;

        RtpMap(int payload_type, 
               std::string codec, 
               int clock_rate, 
               std::optional<std::string> codec_params = std::nullopt);
    };

    void AddRtpMap(RtpMap map);

private:
    static std::optional<RtpMap> ParseRtpMap(const std::string_view& attr_value);
    std::string FormatDescription() const override;
    virtual std::string GenerateSDPLines(const std::string eol) const override;
    std::string GenerateSsrcEntrySDPLines(const SsrcEntry& entry, const std::string eol) const;
    
private:
    Direction direction_;
    
    std::map<int, RtpMap> rtp_maps_;
    std::vector<uint32_t> media_ssrcs_;
    std::vector<uint32_t> rtx_ssrcs_;
    std::vector<uint32_t> fec_ssrcs_;
    std::map<uint32_t, SsrcEntry> ssrc_entries_;

    std::vector<std::string> extra_attributes_;

    int bandwidth_max_value_ = -1;
};

} // namespace sdp
} // namespace naivert 

#endif