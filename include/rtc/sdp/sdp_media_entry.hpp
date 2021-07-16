#ifndef _RTC_SDP_MEDIA_ENTRY_H_
#define _RTC_SDP_MEDIA_ENTRY_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT MediaEntry : public Entry {
public:
    enum class Type {
        AUDIO,
        VIDEO,
        APPLICATION
    };
public:
    virtual ~MediaEntry() = default;

    virtual std::string description() const { return description_; }
    virtual void ParseSDPLine(std::string_view line);

    Type type() const { return type_; }
    std::string type_string() const { return type_string_; }
    std::string mid() const { return mid_; };
    Direction direction() const { return direction_; }
    void set_direction(Direction direction);

    std::string GenerateSDP(std::string_view eol, std::string_view addr, std::string_view port) const;

protected:
    MediaEntry(const std::string& mline, std::string mid, Direction direction = Direction::UNKNOWN);
    virtual std::string GenerateSDPLines(std::string_view eol) const;   

    std::vector<std::string> attributes_;

    Type type_string_to_type(std::string type_string) const;

    void set_fingerprint(std::string fingerprint);

private:
    Type type_;
    std::string type_string_;
    std::string description_;
    std::string mid_;
    Direction direction_;

    // Attributes below appear in both session-level and media-leval
    std::optional<Role> role_ = std::nullopt;
    // ICE attribute
    // See https://tools.ietf.org/id/draft-ietf-mmusic-ice-sip-sdp-14.html#rfc.section.5.4
    std::optional<std::string> ice_ufrag_ = std::nullopt;
    std::optional<std::string> ice_pwd_ = std::nullopt;
    // DTLS attribute
    std::optional<std::string> fingerprint_ = std::nullopt;
};

} // namespace sdp
} // namespace naivert 

#endif