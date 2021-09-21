#ifndef _RTC_SDP_MEDIA_ENTRY_MEDIA_H_
#define _RTC_SDP_MEDIA_ENTRY_MEDIA_H_

#include "rtc/sdp/sdp_media_entry.hpp"

#include <map>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Media : public MediaEntry {
public:
    struct RTPMap {
    public:
        RTPMap(std::string_view mline);
        RTPMap() {}

        void AddFeedback(const std::string line);
        void RemoveFeedback(const std::string& line);
        void AddAttribute(std::string attr) { fmt_profiles.emplace_back(std::move(attr)); }

        static int ParsePayloadType(std::string_view pt);
        void SetMLine(std::string_view mline);

    public:
        std::vector<std::string> rtcp_feedbacks;
        std::vector<std::string> fmt_profiles;
        int pt;
        std::string format;
        int clock_rate;
        std::string codec_params;
    };
public:
    Media(); // For Template in TaskQueue
    Media(const MediaEntry& entry, Direction direction);
    Media(MediaEntry&& entry, Direction direction);
    Media(Type type, 
          std::string mid, 
          const std::string protocols,
          Direction direction = Direction::SEND_ONLY);
    virtual ~Media() = default;

    Direction direction() const;
    void set_direction(Direction direction);

    Media reciprocate() const;

    void set_bandwidth_max_value(int value);
    int bandwidth_max_value();

    void AddSSRC(uint32_t ssrc, std::optional<std::string> name, std::optional<std::string> msid = std::nullopt, std::optional<std::string> track_id = std::nullopt);
    void RemoveSSRC(uint32_t ssrc);
    void ReplaceSSRC(uint32_t old_ssrc, uint32_t ssrc, std::optional<std::string> name, std::optional<std::string> msid = std::nullopt, std::optional<std::string> track_id = std::nullopt);
    bool HasSSRC(uint32_t ssrc);
    std::vector<uint32_t> GetSSRCS();
    std::optional<std::string> GetCNameForSSRC(uint32_t ssrc);

    bool HasPayloadType(int pt) const;

    virtual bool ParseSDPLine(std::string_view line) override;
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;

    void AddRTPMap(const RTPMap& map);

private:
    std::string MediaDescription() const override;
    virtual std::string GenerateSDPLines(const std::string eol) const override;

private:
    Direction direction_;
    
    std::map<int, RTPMap> rtp_map_;
    std::vector<uint32_t> ssrcs_;
    std::map<uint32_t, std::string> cname_map_;

    int bandwidth_max_value_ = -1;
};

} // namespace sdp
} // namespace naivert 

#endif