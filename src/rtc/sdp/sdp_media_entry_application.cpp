#include "rtc/sdp/sdp_media_entry_application.hpp"

#include "common/utils.hpp"

#include <sstream>

namespace naivertc {
namespace sdp {

Application::Application(const std::string mid)
    : MediaEntry("application 9 UDP/DTLS/SCTP", std::move(mid), Direction::SEND_RECV) {}

std::string Application::description() const {
    return MediaEntry::description() + " webrtc-datachannel";
}

Application Application::reciprocate() const {
    Application reciprocate(*this);
    reciprocate.max_message_size_.reset();
    return reciprocate;
}

void Application::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);

        if (key == "sctp-port") {
            sctp_port_ = utils::string::to_integer<uint16_t>(value);
        }else if (key == "max-message-size") {
            max_message_size_ = utils::string::to_integer<size_t>(value);
        }else {
            MediaEntry::ParseSDPLine(line);
        }
    }else {
        MediaEntry::ParseSDPLine(line);
    }
}

std::string Application::GenerateSDPLines(std::string_view eol) const {
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