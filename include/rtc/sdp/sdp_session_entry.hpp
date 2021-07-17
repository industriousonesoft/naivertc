#ifndef _RTC_SDP_SESSION_ENTRY_H_
#define _RTC_SDP_SESSION_ENTRY_H_

#include "rtc/sdp/sdp_entry.hpp"

#include <string>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT SessionEntry : public Entry {
public:
    SessionEntry();
    virtual ~SessionEntry() = default;

    const std::string user_name() const;
    const std::string session_id() const;

    void ParseSDPLine(std::string_view line) override;
    std::string GenerateSDP(std::string_view eol, Role role) const override;

private:
    std::string user_name_;
    std::string session_id_;

};

} // namespace sdp
} // namespace naivert 

#endif