#ifndef _RTC_SDP_MEDIA_ENTRY_H_
#define _RTC_SDP_MEDIA_ENTRY_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <iostream>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT MediaEntry : public Entry {
public:
    enum class Type {
        NONE,
        AUDIO,
        VIDEO,
        APPLICATION
    };
public:
    static MediaEntry Parse(const std::string& mline, std::string mid);
    virtual ~MediaEntry() = default;

    Type type() const { return type_; }
    std::string mid() const { return mid_; };
    
    virtual bool ParseSDPLine(std::string_view line) override;
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;
    std::string GenerateSDP(const std::string eol, Role role) const override;

protected:
    MediaEntry();
    MediaEntry(Type type, 
               std::string mid, 
               const std::string protocols);

    virtual std::string FormatDescription() const;
    virtual std::string GenerateSDPLines(const std::string eol) const;   

    static Type ToType(std::string_view type_string);
   
private:
    Type type_;
    std::string mid_;
    std::string protocols_;
   
    // ICE attribute
    // See https://tools.ietf.org/id/draft-ietf-mmusic-ice-sip-sdp-14.html#rfc.section.5.4
    std::optional<std::string> ice_ufrag_ = std::nullopt;
    std::optional<std::string> ice_pwd_ = std::nullopt;
    // DTLS attribute
    std::optional<std::string> fingerprint_ = std::nullopt;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaEntry::Type type);

} // namespace sdp
} // namespace naivert 

#endif