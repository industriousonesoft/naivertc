#ifndef _RTC_SDP_ENTRY_H_
#define _RTC_SDP_ENTRY_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"

#include <string>
#include <optional>

namespace naivertc {
namespace sdp {

struct Entry : public std::enable_shared_from_this<Entry> {
public:
    enum class Type {
        SESSION,
        MEDIA
    };
public:
    virtual ~Entry() = default;

    std::optional<std::string> ice_ufrag() const;
    std::optional<std::string> ice_pwd() const;
    std::optional<std::string> fingerprint() const;
    std::optional<sdp::Role> role() const;

    void set_ice_ufrag(const std::string ice_ufrag);
    void set_ice_pwd(const std::string ice_pwd);
    void set_fingerprint(std::string fingerprint);
    void set_role(Role role);

    void Hint(const Entry& other);

    virtual bool ParseSDPLine(std::string_view line);
    virtual bool ParseSDPAttributeField(std::string_view key, std::string_view value);
    virtual std::string GenerateSDP(const std::string eol, Role role) const;
protected:
    Entry();
private:
    // Attributes below can appear at either the session-level or media-level
    // ICE attribute
    // See https://tools.ietf.org/id/draft-ietf-mmusic-ice-sip-sdp-14.html#rfc.section.5.4
    std::optional<std::string> ice_ufrag_ = std::nullopt;
    std::optional<std::string> ice_pwd_ = std::nullopt;
    // DTLS attribute
    std::optional<std::string> fingerprint_ = std::nullopt;
    std::optional<sdp::Role> role_ = std::nullopt;

};

} // namespace sdp
} // namespace naivert 

#endif