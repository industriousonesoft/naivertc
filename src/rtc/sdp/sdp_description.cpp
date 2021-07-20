#include "rtc/sdp/sdp_description.hpp"
#include "common/utils.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "rtc/sdp/sdp_media_entry_audio.hpp"
#include "rtc/sdp/sdp_media_entry_video.hpp"

#include <plog/Log.h>

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {

Description::Description(Type type, Role role, std::optional<std::string> ice_ufrag, std::optional<std::string> ice_pwd, std::optional<std::string> fingerprint) 
    : type_(type),
    role_(Role::ACT_PASS) {

    HintRole(role);

    if (ice_ufrag.has_value() && ice_pwd.has_value()) {
        session_entry_.set_ice_ufrag(ice_ufrag.value());
        session_entry_.set_ice_pwd(ice_pwd.value());
    }
    if (fingerprint.has_value()) {
        session_entry_.set_fingerprint(fingerprint.value());
    }
}

Description::~Description() {
    media_entries_.clear();
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

void Description::HintType(Type type) {
    if (type_ == Type::UNSPEC) {
        type_ = type;
        HintRole(role_);
    }
}

void Description::HintRole(Role role) {
    if (type_ == Type::ANSWER && role == Role::ACT_PASS) {
        // ActPass is illegal for an answer, so reset to Passive
        role_ = Role::PASSIVE;
    }else {
        role_ = role;
    }
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

void Description::AddApplication(Application app) {
   AddApplication(std::make_shared<Application>(std::move(app)));
}

void Description::AddApplication(std::shared_ptr<Application> app) {
    // Update ICE and DTLS attributes
    app->Hint(session_entry_);
    media_entries_.emplace_back(app);
}

void Description::AddMedia(Media media) {
    AddMedia(std::make_shared<Media>(std::move(media)));
}

void Description::AddMedia(std::shared_ptr<Media> media) {
    // Update ICE and DTLS attributes
    media->Hint(session_entry_);
    media_entries_.emplace_back(media);
}

void Description::AddApplication(std::string mid) {
    AddApplication(Application(std::move(mid)));
}

void Description::AddAudio(std::string mid, Direction direction) {
    AddMedia(Audio(std::move(mid), direction));
}

void Description::AddVideo(std::string mid, Direction direction) {
    AddMedia(Video(std::move(mid), direction));
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