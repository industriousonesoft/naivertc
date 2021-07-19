#ifndef _RTC_SDP_MEDIA_ENTRY_APPLICATION_H_
#define _RTC_SDP_MEDIA_ENTRY_APPLICATION_H_

#include "rtc/sdp/sdp_media_entry.hpp"

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Application : public MediaEntry {
public:
    Application(const std::string& mline, const std::string mid);
    Application(const std::string mid);
    virtual ~Application() = default;

    std::string description() const override;
    Application reciprocate() const;

    std::optional<uint16_t> sctp_port() const { return sctp_port_; }
    void set_sctp_port(uint16_t port) { sctp_port_ = port; }
    void HintSctpPort(uint16_t port) { sctp_port_ = sctp_port_.value_or(port); }

    std::optional<size_t> max_message_size() const { return max_message_size_; }
    void set_max_message_size(size_t size) { max_message_size_ = size; }

    bool ParseSDPLine(std::string_view line) override;
    bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;

private:
    virtual std::string GenerateSDPLines(std::string_view eol) const override;

    std::optional<uint16_t> sctp_port_;
    std::optional<size_t> max_message_size_;
};
    
} // namespace sdp
} // namespace naivertc

#endif