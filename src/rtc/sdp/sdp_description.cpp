#include "rtc/sdp/sdp_description.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "rtc/sdp/sdp_media_entry_audio.hpp"
#include "rtc/sdp/sdp_media_entry_video.hpp"

#include <plog/Log.h>

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {

Description::Description(Type type, 
                         Role role, 
                         std::optional<std::string> ice_ufrag, 
                         std::optional<std::string> ice_pwd, 
                         std::optional<std::string> fingerprint) 
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
    medias_.clear();
}

Type Description::type() const {
    return type_;
}

Role Description::role() const {
    return role_;
}

const std::string Description::bundle_id() const {
    // Compatible with WebRTC: Get the mid of the first media
    return !media_entries_.empty() ? media_entries_.begin()->first : "0";
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

void Description::ClearMediaEntries() {
    medias_.clear();
    application_.reset();
    media_entries_.clear();
}

bool Description::HasMedia() const {
    return medias_.size() > 0;
}

bool Description::HasAudio() const {
    for (const auto& entry : medias_) {
        if (entry->type() == sdp::MediaEntry::Type::AUDIO) {
            return true;
        } 
    }
    return false;
}

bool Description::HasVideo() const {
    for (const auto& entry : medias_) {
        if (entry->type() == sdp::MediaEntry::Type::VIDEO) {
            return true;
        } 
    }
    return false;
}

bool Description::HasMid(const std::string_view mid) const {
    for (const auto& entry : medias_) {
        if (entry->mid() == mid) {
            return true;
        } 
    }
    return application_ != nullptr ? application_->mid() == mid : false;
}

bool Description::HasApplication() const {
    return application_ != nullptr;
}

const Application* Description::application() const {
    return application_.get();
}

Application* Description::application() {
    return application_.get();
}

Application* Description::SetApplication(Application app) {
    application_ = std::make_shared<Application>(std::move(app));
    // Update ICE and DTLS attributes
    application_->Hint(session_entry_);
    media_entries_.emplace(application_->mid(), application_);
    return application_.get();
}

Media* Description::AddMedia(Media media) {
    auto new_media = std::make_shared<Media>(std::move(media));
    // Update ICE and DTLS attributes
    new_media->Hint(session_entry_);
    medias_.emplace_back(new_media);
    media_entries_.emplace(new_media->mid(), new_media);
    return new_media.get();
}

void Description::RemoveMedia(const std::string_view mid) {
    for (auto it = medias_.begin(); it != medias_.end();) {
        auto media = std::shared_ptr<Media>(*it);
        if (media->mid() == mid) {
            medias_.erase(it);
        }else {
            it++;
        }
    }
}

void Description::ForEach(std::function<void(const Media&)> handler) const {
    if (!handler) {
        return;
    }
    for (const auto& media : medias_) {
        handler(*media.get());
    }
}

const Media* Description::media(std::string_view mid) const {
    for (const auto& entry : medias_) {
        if (entry->mid() == mid) {
            return entry.get();
        }
    }
    return nullptr;
}

Media* Description::media(std::string_view mid) {
    for (auto& entry : medias_) {
        if (entry->mid() == mid) {
            return entry.get();
        }
    }
    return nullptr;
}

Description::operator std::string() const {
    return GenerateSDP("\r\n");
}

// GenerateSDP
std::string Description::GenerateSDP(const std::string eol, bool application_only) const {
    // warning: Be careful, there is no space after '=' and only has one space between two parts in a line.
    std::ostringstream oss;
    const std::string sp = " ";
    
    // Session-level lines
    oss << session_entry_.GenerateSDP(eol, role_);

    // 除了data channel之外还有音视频流时设置此属性，共用一个传输通道传输的媒体，
    // 如果没有设置该属性，音、视频、data channel就会分别单独用一个udp端口来传输数据
    if (!application_only) {
        // Bundle (RFC8843 Negotiating Media Multiplexing Using the Session Description Protocol)
        // https://tools.ietf.org/html/rfc8843
        // eg: a=group:BUNDLE audio video data 
        oss << "a=group:BUNDLE";
        for (const auto& kv : media_entries_) {
            oss << sp << kv.first;
        }
        oss << eol;
    }
    
    // WMS是WebRTC Media Stram的缩写，这里给Media Stream定义了一个唯一的标识符。
    // 一个Media Stream可以有多个track（video track、audio track），
    // 这些track就是通过这个唯一标识符关联起来的，具体见下面的媒体行(m=)以及它对应的附加属性(a=ssrc:)
    // 可以参考这里 http://tools.ietf.org/html/draft-ietf-mmusic-msid
    oss << "a=msid-semantic:" << sp << "WMS" << eol;
      
    // Media entries lines
    for (const auto& [key, value] : media_entries_) {
        if (auto entry = value.lock()) {
            if (application_only && entry->type() != MediaEntry::Type::APPLICATION) {
                continue;
            }
            oss << entry->GenerateSDP(eol, role_);
        }
    }

    return oss.str();
}

} // namespace sdp
} // namespace naivertc