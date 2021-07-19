#include "rtc/sdp/sdp_entry.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {

Entry::Entry() {

}

std::optional<std::string> Entry::ice_ufrag() const {
    return ice_ufrag_;
}

std::optional<std::string> Entry::ice_pwd() const {
    return ice_pwd_;
}

std::optional<std::string> Entry::fingerprint() const {
    return fingerprint_;
}

std::string Entry::GenerateSDP(std::string_view eol, Role role) const {
    std::ostringstream oss;
    const std::string sp = " ";

    // The "ice-pwd" and "ice-ufrag" attributes can appear at either the session-level or media-level.
    // When present in both, the value in the media-level takes precedence. Thus, the value at the session-level
    // is effectively a default that applies to all media streams, unless overridden by a media-level value.
    // See https://tools.ietf.org/id/draft-ietf-mmusic-ice-sip-sdp-14.html#rfc.section.5.4
    if (ice_ufrag_.has_value() && ice_pwd_.has_value()) {
        oss << "a=ice-ufrag:" << ice_ufrag_.value() << eol;
        oss << "a=ice-pwd:" << ice_pwd_.value() << eol;

        // FIXME: Can 'ice-options' attribute appear alone(without ice-ufrag and ice-pwd)?
        // Offer and Answer can contain any set of candidates, A trickle ICE session (including
        // the "trickle" token for the "a=ice-options" attribute) MAY contain no candidates at all
        // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-trickle-ice-02#section-5.1
        oss << "a=ice-options:trickle" << eol;
    }

    if (fingerprint_.has_value()) {
        oss << "a=fingerprint:sha-256" << sp << fingerprint_.value() << eol;
    }

    // The role in DTLS negotiation, See rfc4145 rfc4572
    oss << "a=setup:" << RoleToString(role) << eol;

    return oss.str();
}

void Entry::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);
        if (key == "fingerprint") {
            auto fingerprint = ParseFingerprintAttribute(value);
            if (fingerprint.has_value()) {
                set_fingerprint(std::move(fingerprint.value()));
            }else {
                PLOG_WARNING << "Failed to parse fingerprint format: " << value;
            }
        }else if (key == "ice-ufrag") {
            ice_ufrag_.emplace(std::move(value));
        }else if (key == "ice-pwd") {
            ice_pwd_.emplace(std::move(value));
        }else if (key == "candidate") {
            // TODO：add candidate from sdp
        }else if (key == "end-of-candidate") {
            // TODO：add candidate from sdp
        }
    }
}

void Entry::set_fingerprint(std::string fingerprint) {

    if (!IsSHA256Fingerprint(fingerprint)) {
        throw std::invalid_argument("Invalid SHA265 fingerprint: " + fingerprint);
    }

    // make sure All the chars in finger print is uppercase.
    std::transform(fingerprint.begin(), fingerprint.end(), fingerprint.begin(), [](char c) {
        return char(std::toupper(c));
    });

    fingerprint_.emplace(std::move(fingerprint));
}

} // namespace sdp
} // namespace naivert 