#ifndef _RTC_SDP_MEDIA_ENTRY_MEDIA_H_
#define _RTC_SDP_MEDIA_ENTRY_MEDIA_H_

#include "rtc/sdp/sdp_media_entry.hpp"

#include <map>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Media : public MediaEntry {
public:
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

    struct SsrcEntry {
        uint32_t ssrc = 0;
        std::optional<std::string> cname = std::nullopt;
        std::optional<std::string> msid = std::nullopt;
        std::optional<std::string> track_id = std::nullopt;

        SsrcEntry(uint32_t ssrc, 
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

    std::vector<uint32_t> ssrcs() const { return ssrcs_; };
    std::optional<std::string> CNameForSsrc(uint32_t ssrc);
    bool HasSsrc(uint32_t ssrc);

    void AddSsrcEntry(SsrcEntry ssrc_entry);
    void RemoveSsrcEntry(uint32_t ssrc);
    void ReplaceSsrcEntry(uint32_t old_ssrc, SsrcEntry new_ssrc_entry);
    
    bool AddFeedback(int payload_type, const std::string feed_back);

    bool HasPayloadType(int pt) const;

    virtual bool ParseSDPLine(std::string_view line) override;
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;

    Media reciprocate() const;

    void AddRtpMap(const RtpMap& map);

private:
    std::string MediaDescription() const override;
    virtual std::string GenerateSDPLines(const std::string eol) const override;

    static std::optional<RtpMap> Parse(const std::string_view& attr_value);
private:
    Direction direction_;
    
    std::map<int, RtpMap> rtp_map_;
    std::vector<uint32_t> ssrcs_;
    std::map<uint32_t, SsrcEntry> ssrc_entries_;

    std::vector<std::string> extra_attributes_;

    int bandwidth_max_value_ = -1;
};

} // namespace sdp
} // namespace naivert 

#endif