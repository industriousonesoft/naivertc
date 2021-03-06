#include "rtc/sdp/sdp_description.hpp"
#include "common/utils_string.hpp"
#include "rtc/sdp/sdp_media_entry.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {

Description Description::Parser::Parse(const std::string& sdp, Type type) {
    auto description = Description(type, Role::ACT_PASS /* The role value will be updated later */);
    int index = -1;
    std::istringstream iss(sdp);
    MediaEntry* curr_entry = nullptr;
    while (iss) {
        std::string line;
        std::getline(iss, line);
        utils::string::trim_begin(line);
        utils::string::trim_end(line);
        if (line.empty())
            continue;

        // Media-level lines
        if (utils::string::match_prefix(line, "m=")) {
            auto mline = line.substr(2);
            auto sp = " ";
            std::string type = mline.substr(0, mline.find(sp));
            if (type == "application") {
                Application app(MediaEntry::Parse(mline, std::to_string(++index)));
                description.SetApplication(std::move(app));
                curr_entry = description.application();
            } else {
                Media media(MediaEntry::Parse(mline, std::to_string(++index)), Direction::INACTIVE);
                const std::string mid = media.mid();
                description.AddMedia(std::move(media));
                curr_entry = description.media(mid);
            }
        }
        // attributes which can appear at either session-level or media-level
        else if (utils::string::match_prefix(line, "a=")) {
            std::string attr = line.substr(2);
            auto [key, value] = utils::string::parse_pair(attr);

            // media-level takes precedence
            if (curr_entry && curr_entry->ParseSDPAttributeField(key, value)) {
                // Update ICE and DTLS settings 
                if (key == "ice-ufrag" && curr_entry->ice_ufrag()) {
                    description.session_entry_.set_ice_ufrag(curr_entry->ice_ufrag().value());
                } else if (key == "ice-pwd" && curr_entry->ice_pwd()) {
                    description.session_entry_.set_ice_pwd(curr_entry->ice_pwd().value());
                } else if (key == "setup" && curr_entry->role()) {
                    description.HintRole(curr_entry->role().value());
                } else if (key == "fingerprint" && curr_entry->fingerprint()) {
                    description.session_entry_.set_fingerprint(curr_entry->fingerprint().value());
                }
            }
            // session-level
            else if (description.session_entry_.ParseSDPAttributeField(key, value)) {
                // Do nothing
            } else if (curr_entry && curr_entry->ParseSDPLine(line)) {
                // Do nothing
            } else if (description.session_entry_.ParseSDPLine(line)){
                // Do nothing
            // Global attributes
            } else {
                // extmap-allow-mixed
                if (value == "extmap-allow-mixed") {
                    description.set_extmap_allow_mixed(true);
                }
                PLOG_WARNING << "Unknown attribute: [" << key << ":" << value << "]";
            }
        } else  {
            // media-level takes precedence
            if (curr_entry && curr_entry->ParseSDPLine(line)) {
                // Do nothing
            } else if (description.session_entry_.ParseSDPLine(line)){
                // Do nothing
            } else {
                PLOG_WARNING << "Unknown filed: " << line;
            }
        }
        
    } // end of while

    return description;
}

    
} // namespace sdp
} // namespace naivertc
