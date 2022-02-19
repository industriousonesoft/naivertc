#ifndef _RTC_SDP_SESSION_ENTRY_H_
#define _RTC_SDP_SESSION_ENTRY_H_

#include "rtc/sdp/sdp_entry.hpp"

#include <string>

namespace naivertc {
namespace sdp {

struct SessionEntry : public Entry {
public:
    SessionEntry();
    virtual ~SessionEntry() = default;

    const std::string user_name() const;
    const std::string session_id() const;

    bool ParseSDPLine(std::string_view line) override;
    bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;
    std::string GenerateSDP(const std::string eol, Role role) const override;

private:
    std::string user_name_;
    std::string session_id_;

};

} // namespace sdp
} // namespace naivert 

#endif