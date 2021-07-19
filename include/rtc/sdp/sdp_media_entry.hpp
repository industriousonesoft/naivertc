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

    Type type() const { return type_; }
    std::string type_string() const { return type_string_; }
    std::string mid() const { return mid_; };
    
    virtual std::string description() const { return description_; }
    virtual bool ParseSDPLine(std::string_view line) override;
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;
    std::string GenerateSDP(std::string_view eol, Role role) const override;

protected:
    MediaEntry(const std::string& mline, const std::string mid);

    virtual std::string GenerateSDPLines(std::string_view eol) const;   

    Type type_string_to_type(std::string_view type_string) const;
   
    std::vector<std::string> attributes_;

private:
    Type type_;
    std::string type_string_;
    std::string description_;
    std::string mid_;

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