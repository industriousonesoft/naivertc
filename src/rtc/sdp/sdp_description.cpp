#include "rtc/sdp/sdp_description.hpp"
#include "common/utils.hpp"
#include "rtc/sdp/sdp_utils.hpp"

#include <plog/Log.h>

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {
Description::Description(const std::string& sdp, Type type, Role role) : 
    type_(Type::UNSPEC),
    role_(role) {
        
    hintType(type);
    Parse(sdp);
}

Description::Description(const std::string& sdp, const std::string& type_string) : 
    Description(sdp, StringToType(type_string), Role::ACT_PASS) {
}

Type Description::type() const {
    return type_;
}

Role Description::role() const {
    return role_;
}

const std::string Description::bundle_id() const {
    return !media_entries_.empty() ? media_entries_[0]->mid() : "0";
}

std::optional<std::string> Description::ice_ufrag() const {
    return session_entry_.ice_ufrag();
}

std::optional<std::string> Description::ice_pwd() const {
    return session_entry_.ice_pwd();
}

std::optional<std::string> Description::fingerprint() const {
    return session_entry_.fingerprint();
}

void Description::hintType(Type type) {
    if (type_ == Type::UNSPEC) {
        type_ = type;
        if (type_ == Type::ANSWER && role_ == Role::ACT_PASS) {
            // ActPass is illegal for an answer, so reset to Passive
            role_ = Role::PASSIVE;
        }
    }
}

void Description::set_fingerprint(std::string fingerprint) {
    session_entry_.set_fingerprint(std::move(fingerprint));
}

int Description::AddMedia(Media media) {
    media_entries_.emplace_back(std::make_shared<Media>(std::move(media)));
    return int(media_entries_.size()) - 1;
}

int Description::AddApplication(Application app) {
    media_entries_.emplace_back(std::make_shared<Application>(std::move(app)));
    return int(media_entries_.size()) - 1;
}

int Description::AddApplication(std::string mid) {
    return AddApplication(Application(std::move(mid)));
}

int Description::AddAudio(std::string mid, Direction direction) {
    return AddMedia(Audio(std::move(mid), direction));
}

int Description::AddVideo(std::string mid, Direction direction) {
    return AddMedia(Video(std::move(mid), direction));
}

void Description::ClearMedia() {
    media_entries_.clear();
}

bool Description::HasApplication() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::APPLICATION) {
            return true;
        } 
    }
    return false;
}

bool Description::HasAudio() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::AUDIO) {
            return true;
        } 
    }
    return false;
}

bool Description::HasVieo() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::VIDEO) {
            return true;
        } 
    }
    return false;
}

bool Description::HasMid(std::string_view mid) const {
    for (auto entry : media_entries_) {
        if (entry->mid() == mid) {
            return true;
        } 
    }
    return false;
}

Description::operator std::string() const {
    return GenerateSDP("\r\n");
}

// GenerateSDP
std::string Description::GenerateSDP(std::string_view eol, bool application_only) const {
    // warning: Be careful, there is no space after '=' and only has one space between two parts in a line.
    std::ostringstream oss;
    const std::string sp = " ";
    
    // Session-level lines
    oss << session_entry_.GenerateSDP(eol, role_);

    // 除了data channel之外还有音视频流时设置此属性，共用一个传输通道传输的媒体，
    // 如果没有设置该属性，音、视频、data channel就会分别单独用一个udp端口来传输数据
    if (application_only == false) {
        // Bundle (RFC8843 Negotiating Media Multiplexing Using the Session Description Protocol)
        // https://tools.ietf.org/html/rfc8843
        // eg: a=group:BUNDLE audio video data 
        oss << "a=group:BUNDLE";
        for (const auto &entry : media_entries_) {
            oss << sp << entry->mid();
        }
        oss << eol;
    }
    
    // WMS是WebRTC Media Stram的缩写，这里给Media Stream定义了一个唯一的标识符。
    // 一个Media Stream可以有多个track（video track、audio track），
    // 这些track就是通过这个唯一标识符关联起来的，具体见下面的媒体行(m=)以及它对应的附加属性(a=ssrc:)
    // 可以参考这里 http://tools.ietf.org/html/draft-ietf-mmusic-msid
    oss << "a=msid-semantic:" << sp << "WMS" << eol;
        
     // Media-level lines
    for (const auto& entry : media_entries_) {
        if (application_only && entry->type() != MediaEntry::Type::APPLICATION) {
            continue;
        }
        oss << entry->GenerateSDP(eol, role_);
    }

    return oss.str();
}

// private methods
void Description::Parse(const std::string& sdp) {
    int index = -1;
    std::istringstream ss(sdp);
    std::shared_ptr<MediaEntry> curr_entry;
    while (ss) {
        std::string line;
        std::getline(ss, line);
        utils::string::trim_end(line);
        if (line.empty())
            continue;

        // Media-level lines
        if (utils::string::match_prefix(line, "m=")) {
            curr_entry = CreateMediaEntry(line.substr(2), std::to_string(++index), Direction::UNKNOWN);
        }
        // attributes which can appear at either session-level or media-level
        else if (utils::string::match_prefix(line, "a=")) {

            std::string attr = line.substr(2);
            auto [key, value] = utils::string::parse_pair(attr);
            // 'setup' attribute parsed at session-level
            if (key == "setup") {
                if (value == "active") {
                    role_ = Role::ACTIVE;
                }else if (value == "passive") {
                    role_ = Role::PASSIVE;
                }else {
                    role_ = Role::ACT_PASS;
                }
            }
            // media-level takes precedence
            else if (curr_entry) {
                curr_entry->ParseSDPLine(std::move(line));
            }
            // session-level
            else {
                session_entry_.ParseSDPLine(std::move(line));
            }
        }else if (curr_entry) {
            curr_entry->ParseSDPLine(std::move(line));
        }
        // Session-level lines
        else {
            session_entry_.ParseSDPLine(std::move(line));
        }
    } // end of while
}

std::shared_ptr<MediaEntry> Description::CreateMediaEntry(const std::string& mline, const std::string mid, Direction direction) {
    std::string type = mline.substr(0, mline.find(' '));
    if (type == "application") {
        auto app = std::make_shared<Application>(std::move(mid));
        media_entries_.emplace_back(app);
        return app;
    }else {
        auto media = std::make_shared<Media>(mline, std::move(mid), direction);
        media_entries_.emplace_back(media);
        return media;
    }
}

std::variant<Media*, Application*> Description::media(unsigned int index) {
    if (index >= media_entries_.size()) {
        throw std::out_of_range("Media index out of range.");
    }

    const auto& entry = media_entries_[index];
    if (entry->type() == MediaEntry::Type::APPLICATION) {
        auto app = dynamic_cast<Application*>(entry.get());
        if (!app) {
            throw std::logic_error("Bad type of application in description.");
        }
        return app;
    }else {
        auto media = dynamic_cast<Media*>(entry.get());
        if (!media) {
            throw std::logic_error("Bad type of media in description.");
        }
        return media;
    }
}

std::variant<const Media*, const Application*> Description::media(unsigned int index) const {
     if (index >= media_entries_.size()) {
        throw std::out_of_range("Media index out of range.");
    }

    const auto& entry = media_entries_[index];
    if (entry->type() == MediaEntry::Type::APPLICATION) {
        auto app = dynamic_cast<Application*>(entry.get());
        if (!app) {
            throw std::logic_error("Bad type of application in description.");
        }
        return app;
    }else {
        auto media = dynamic_cast<Media*>(entry.get());
        if (!media) {
            throw std::logic_error("Bad type of media in description.");
        }
        return media;
    }
}

unsigned int Description::media_count() const {
    return unsigned(media_entries_.size());
}

const Application* Description::application() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::APPLICATION) {
            return static_cast<Application* >(entry.get());
        } 
    }
    return nullptr;
}

Application* Description::application() {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::APPLICATION) {
            return static_cast<Application* >(entry.get());
        } 
    }
    return nullptr;
}


} // namespace sdp
} // namespace naivertc