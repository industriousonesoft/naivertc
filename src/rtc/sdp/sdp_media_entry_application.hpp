#ifndef _RTC_SDP_MEDIA_ENTRY_APPLICATION_H_
#define _RTC_SDP_MEDIA_ENTRY_APPLICATION_H_

#include "rtc/sdp/sdp_media_entry.hpp"

namespace naivertc {
namespace sdp {

struct Application : public MediaEntry {
public:
    Application(const MediaEntry& entry);
    Application(MediaEntry&& entry);
    Application(std::string mid);
    ~Application();

    std::optional<uint16_t> sctp_port() const { return sctp_port_; }
    void set_sctp_port(uint16_t port) { sctp_port_ = port; }
    void HintSctpPort(uint16_t port) { sctp_port_ = sctp_port_.value_or(port); }

    std::optional<size_t> max_message_size() const { return max_message_size_; }
    void set_max_message_size(size_t size) { max_message_size_ = size; }

    bool ParseSDPLine(std::string_view line) override;
    bool ParseSDPAttributeField(std::string_view key, std::string_view value) override;

    Application Reciprocated() const;

private:
    std::string ExtraMediaInfo() const override;
    std::string GenerateSDPLines(const std::string eol) const override;

    std::optional<uint16_t> sctp_port_;
    std::optional<size_t> max_message_size_;
};
    
} // namespace sdp
} // namespace naivertc

#endif