#include "rtc/sdp/sdp_media_entry_application.hpp"
#include "common/utils_string.hpp"

#include <sstream>

namespace naivertc {
namespace sdp {

Application::Application(const MediaEntry& entry) 
    : MediaEntry(entry) {}

Application::Application(MediaEntry&& entry)
    : MediaEntry(entry) {}

Application::Application(std::string mid)
    : MediaEntry(Kind::APPLICATION, std::move(mid), "UDP/DTLS/SCTP") {}

Application::~Application() {}

std::string Application::ExtraMediaInfo() const {
    // TODO: Add a=sctpmap attribute support
    // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-sctp-sdp-06#section-5.1
    return "webrtc-datachannel";
}

Application Application::Reciprocated() const {
    Application reciprocated(*this);
    // FIXME: I think reserving those properties as default value is better, as we can change them anytime.
    // reciprocated.sctp_port_.reset();
    // reciprocated.max_message_size_.reset();
    return reciprocated;
}

bool Application::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);
        return ParseSDPAttributeField(key, value);
    } else {
        return MediaEntry::ParseSDPLine(line);
    }
}

bool Application::ParseSDPAttributeField(std::string_view key, std::string_view value) {
    if (key == "sctp-port") {
        sctp_port_ = utils::string::to_integer<uint16_t>(value);
        return true;
    } else if (key == "max-message-size") {
        max_message_size_ = utils::string::to_integer<size_t>(value);
        return true;
    } else {
        return MediaEntry::ParseSDPAttributeField(key, value);
    }
}

std::string Application::GenerateSDPLines(const std::string eol) const {
    std::ostringstream oss;
    oss << MediaEntry::GenerateSDPLines(eol);

    if (sctp_port_) {
        oss << "a=sctp-port:" << *sctp_port_ << eol;
    }

    if (max_message_size_) {
        oss << "a=max-message-size:" << *max_message_size_ << eol;
    }

    return oss.str();
}
    
} // namespace sdp
} // namespace naivert 