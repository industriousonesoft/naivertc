#include "rtc/sdp/sdp_description.hpp"
#include "common/utils.hpp"
#include "rtc/sdp/sdp_media_entry.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {

Description Description::Parser::Parse(const std::string& sdp, Type type) {
    auto description = Description(type, Role::ACT_PASS /* The role value will be updated later */);
    int index = -1;
    std::istringstream iss(sdp);
    std::shared_ptr<MediaEntry> curr_entry;
    std::optional<Role> parsed_role;
    SessionEntry session_entry;
    while (iss) {
        std::string line;
        std::getline(iss, line);
        utils::string::trim_end(line);
        if (line.empty())
            continue;

        // Media-level lines
        if (utils::string::match_prefix(line, "m=")) {
            auto mline = line.substr(2);
            std::string type = mline.substr(0, mline.find(' '));
            if (type == "application") {
                auto app = std::make_shared<Application>(mline, std::to_string(++index));
                description.AddApplication(app);
                curr_entry = app;
            }else {
                auto media = std::make_shared<Media>(mline, std::to_string(++index), Direction::UNKNOWN);
                description.AddMedia(media);
                curr_entry = media;
            }
        }
        // attributes which can appear at either session-level or media-level
        else if (utils::string::match_prefix(line, "a=")) {
            std::string attr = line.substr(2);
            auto [key, value] = utils::string::parse_pair(attr);

            // media-level takes precedence
            if (curr_entry && curr_entry->ParseSDPAttributeField(key, value)) {
                if (curr_entry->role()) {
                    parsed_role.emplace(curr_entry->role().value());
                }
            }
            // session-level
            else {
                session_entry.ParseSDPAttributeField(key, value);
            }

        }else  {
            // media-level takes precedence
            if (curr_entry && curr_entry->ParseSDPLine(std::move(line))) {
                // Do nothing
            }else {
                session_entry.ParseSDPLine(std::move(line));
            }
        }
        
    } // end of while

    // ICE settings
    if (session_entry.ice_ufrag().has_value() && session_entry.ice_pwd().has_value()) {
        description.set_ice_ufrag(session_entry.ice_ufrag().value());
        description.set_ice_pwd(session_entry.ice_pwd().value());
    }

    // DTLS fingerprint
    if (session_entry.fingerprint().has_value()) {
        description.set_fingerprint(session_entry.fingerprint().value());
    }

    // DTLS Role
    if (!parsed_role && session_entry.role()) {
        parsed_role.emplace(session_entry.role().value());
    }

    if (parsed_role) {
        description.HintRole(parsed_role.value());
    }

    return description;
}

    
} // namespace sdp
} // namespace naivertc
