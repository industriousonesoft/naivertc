#include "rtc/sdp/sdp_session_description.hpp"
#include "common/utils.hpp"
#include "rtc/sdp/sdp_utils.hpp"

#include <plog/Log.h>

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {
SessionDescription::SessionDescription(const std::string& sdp, Type type, Role role) : 
    type_(Type::UNSPEC),
    role_(role) {
        
    hintType(type);
    Parse(std::move(sdp));
}

SessionDescription::SessionDescription(const std::string& sdp, std::string type_string) : 
    SessionDescription(sdp, StringToType(type_string), Role::ACT_PASS) {
}

Type SessionDescription::type() const {
    return type_;
}

Role SessionDescription::role() const {
    return role_;
}

std::string SessionDescription::bundle_id() const {
    return !media_entries_.empty() ? media_entries_[0]->mid() : "0";
}

std::optional<std::string> SessionDescription::ice_ufrag() const {
    return session_entry_.ice_ufrag();
}

std::optional<std::string> SessionDescription::ice_pwd() const {
    return session_entry_.ice_pwd();
}

std::optional<std::string> SessionDescription::fingerprint() const {
    return session_entry_.fingerprint();
}

void SessionDescription::hintType(Type type) {
    if (type_ == Type::UNSPEC) {
        type_ = type;
        if (type_ == Type::ANSWER && role_ == Role::ACT_PASS) {
            // ActPass is illegal for an answer, so reset to Passive
            role_ = Role::PASSIVE;
        }
    }
}

void SessionDescription::set_fingerprint(std::string fingerprint) {
    session_entry_.set_fingerprint(std::move(fingerprint));
}

int SessionDescription::AddMedia(Media media) {
    media_entries_.emplace_back(std::make_shared<Media>(std::move(media)));
    return int(media_entries_.size()) - 1;
}

int SessionDescription::AddApplication(Application app) {
    media_entries_.emplace_back(std::make_shared<Application>(std::move(app)));
    return int(media_entries_.size()) - 1;
}

int SessionDescription::AddApplication(std::string mid) {
    return AddApplication(Application(std::move(mid)));
}

int SessionDescription::AddAudio(std::string mid, Direction direction) {
    return AddMedia(Audio(std::move(mid), direction));
}

int SessionDescription::AddVideo(std::string mid, Direction direction) {
    return AddMedia(Video(std::move(mid), direction));
}

void SessionDescription::ClearMedia() {
    media_entries_.clear();
}

bool SessionDescription::HasApplication() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::APPLICATION) {
            return true;
        } 
    }
    return false;
}

bool SessionDescription::HasAudio() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::AUDIO) {
            return true;
        } 
    }
    return false;
}

bool SessionDescription::HasVieo() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::VIDEO) {
            return true;
        } 
    }
    return false;
}

bool SessionDescription::HasMid(std::string_view mid) const {
    for (auto entry : media_entries_) {
        if (entry->mid() == mid) {
            return true;
        } 
    }
    return false;
}

SessionDescription::operator std::string() const {
    return GenerateSDP("\r\n");
}

// GenerateSDP
#warning Be careful, there is no space after '=' and only has one space between two parts in a line.
std::string SessionDescription::GenerateSDP(std::string_view eol, bool application_only) const {
    std::ostringstream oss;
    std::string sp = " ";
    
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
        
    for (const auto& entry : media_entries_) {
        if (application_only && entry->type() != MediaEntry::Type::APPLICATION) {
            continue;
        }
        oss << entry->GenerateSDP(eol, role_);
    }
    return oss.str();

}

// private methods
void SessionDescription::Parse(std::string sdp) {
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

std::shared_ptr<MediaEntry> SessionDescription::CreateMediaEntry(std::string mline, std::string mid, Direction direction) {
    std::string type = mline.substr(0, mline.find(' '));
    if (type == "application") {
        auto app = std::make_shared<Application>(std::move(mid));
        media_entries_.emplace_back(app);
        return app;
    }else {
        auto media = std::make_shared<Media>(std::move(mline), std::move(mid), direction);
        media_entries_.emplace_back(media);
        return media;
    }
}

std::variant<Media*, Application*> SessionDescription::media(unsigned int index) {
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

std::variant<const Media*, const Application*> SessionDescription::media(unsigned int index) const {
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

unsigned int SessionDescription::media_count() const {
    return unsigned(media_entries_.size());
}

const Application* SessionDescription::application() const {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::APPLICATION) {
            return static_cast<Application* >(entry.get());
        } 
    }
    return nullptr;
}

Application* SessionDescription::application() {
    for (auto entry : media_entries_) {
        if (entry->type() == sdp::MediaEntry::Type::APPLICATION) {
            return static_cast<Application* >(entry.get());
        } 
    }
    return nullptr;
}


} // namespace sdp
} // namespace naivertc