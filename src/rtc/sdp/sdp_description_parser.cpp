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
                description.media_entries_.emplace_back(app);
                curr_entry = app;
            }else {
                auto media = std::make_shared<Media>(mline, std::to_string(++index), Direction::UNKNOWN);
                description.media_entries_.emplace_back(media);
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
                description.session_entry_.ParseSDPAttributeField(key, value);
            }

        }else  {
            // media-level takes precedence
            if (curr_entry && curr_entry->ParseSDPLine(std::move(line))) {
                // Do nothing
            }else {
                description.session_entry_.ParseSDPLine(std::move(line));
            }
        }
        
    } // end of while

    // Using session-level DTLS role if neccessary
    if (!parsed_role && description.session_entry_.role()) {
        parsed_role.emplace(description.session_entry_.role().value());
    }

    if (parsed_role) {
        description.HintRole(parsed_role.value());
    }

    return description;
}

    
} // namespace sdp
} // namespace naivertc
