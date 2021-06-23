#include "pc/sdp_serializer.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {
Serializer::Serializer(const std::string& sdp, Type type, Role role) : 
    type_(Type::UNSPEC),
    role_(role) {
    hintType(type);

    int index = -1;
    std::istringstream ss(sdp);
    std::shared_ptr<Entry> curr_entry;
    while (ss) {
        std::string line;
        std::getline(ss, line);
        utils::string::trim_end(line);
        if (line.empty())
            continue;

        // m-line
        if (utils::string::match_prefix(line, "m=")) {
            curr_entry = CreateEntry(line.substr(2), std::to_string(++index), Direction::UNKNOWN);
        // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
        }else if (utils::string::match_prefix(line, "o=")) {
            std::istringstream origin(line.substr(2));
            origin >> user_name_ >> session_id_;
        // attribute line
        }else if (utils::string::match_prefix(line, "a=")) {
            std::string attr = line.substr(2);
            auto [key, value] = utils::string::parse_pair(attr);

            if (key == "setup") {
                if (value == "active") {
                    role_ = Role::ACTIVE;
                }else if (value == "passive") {
                    role_ = Role::PASSIVE;
                }else {
                    role_ = Role::ACT_PASS;
                }
            }else if (key == "fingerprint") {
                if (utils::string::match_prefix(value, "sha-265 ")) {
                    std::string finger_print{value.substr(8)};
                    utils::string::trim_begin(finger_print);
                    set_finger_print(finger_print);
                }else {
                    PLOG_WARNING << "Unknown SDP fingerprint format: " << value;
                }
            }else if (key == "ice-ufrag") {
                ice_ufrag_ = value;
            }else if (key == "ice-pwd") {
                ice_pwd_ = value;
            }else if (key == "candidate") {
                // TODO：add candidate from sdp
            }else if (key == "end-of-candidate") {
                // TODO：add candidate from sdp
            }else if (curr_entry) {
                curr_entry->ParseSDPLine(std::move(line));
            }
        }else if (curr_entry) {
            curr_entry->ParseSDPLine(std::move(line));
        }
    } // end of while

    // username如何没有使用'-'代替
    if (user_name_.empty()) {
        user_name_ = "-";
    }

    if (session_id_.empty()) {

    }

}

Serializer::Serializer(const std::string& sdp, std::string type_string) : 
    Serializer(sdp, StringToType(type_string), Role::ACT_PASS) {
}

Type Serializer::type() const {
    return type_;
}

Role Serializer::role() const {
    return role_;
}

void Serializer::hintType(Type type) {
    if (type_ == Type::UNSPEC) {
        type_ = type;
        if (type_ == Type::ANSWER && role_ == Role::ACT_PASS) {
            // ActPass is illegal for an answer, so reset to Passive
            role_ = Role::PASSIVE;
        }
    }
}

void Serializer::set_finger_print(std::string finger_print) {
    if (!utils::string::is_sha256_fingerprint(finger_print)) {
        throw std::invalid_argument("Invalid SHA265 finger print: " + finger_print );
    }

    // make sure All the chars in finger print is uppercase.
    std::transform(finger_print.begin(), finger_print.end(), finger_print.begin(), [](char c) {
        return char(std::toupper(c));
    });

    finger_print_.emplace(std::move(finger_print));
}

int Serializer::AddMedia(Media media) {
    entries_.emplace_back(std::make_shared<Media>(std::move(media)));
    return int(entries_.size()) - 1;
}

int Serializer::AddApplication(Application app) {
    application_.reset();
    application_ = std::make_shared<Application>(std::move(app));
    entries_.emplace_back(application_);
    return int(entries_.size()) - 1;
}

int Serializer::AddApplication(std::string mid) {
    return AddApplication(Application(std::move(mid)));
}

int Serializer::AddAudio(std::string mid, Direction direction) {
    return AddMedia(Audio(std::move(mid), direction));
}

int Serializer::AddVideo(std::string mid, Direction direction) {
    return AddMedia(Video(std::move(mid), direction));
}

void Serializer::ClearMedia() {
    entries_.clear();
    application_.reset();
}

std::string Serializer::GenerateSDP(std::string_view eol) const {
    std::ostringstream sdp;

    // Header
    sdp << "v=0" << eol;
    // sdp << "o=" << 
}


// private methods
std::shared_ptr<Entry> Serializer::CreateEntry(std::string mline, std::string mid, Direction direction) {
    std::string type = mline.substr(0, mline.find(' '));
    if (type == "application") {
        application_.reset();
        application_ = std::make_shared<Application>(std::move(mid));
        entries_.emplace_back(application_);
        return application_;
    }else {
        auto media = std::make_shared<Media>(std::move(mline), std::move(mid), direction);
        entries_.emplace_back(media);
        return media;
    }
}

}
} // end of naive rtc