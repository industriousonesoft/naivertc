#ifndef _RTC_SDP_DESCRIPTION_H_
#define _RTC_SDP_DESCRIPTION_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"
#include "rtc/sdp/sdp_session_entry.hpp"
#include "rtc/sdp/sdp_media_entry_application.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include <rtc/sdp/sdp_media_entry_audio.hpp>
#include <rtc/sdp/sdp_media_entry_video.hpp>

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <optional>

namespace naivertc {
namespace sdp {

// This class is not thread-safe, the caller MUST provide that.
class RTC_CPP_EXPORT Description {
public:
// Builder
class RTC_CPP_EXPORT Builder {
public:
    Builder(Type type);
    ~Builder();

    Builder& set_role(Role role);
    Builder& set_ice_ufrag(std::optional<std::string> ice_ufrag);
    Builder& set_ice_pwd(std::optional<std::string> ice_pwd);
    Builder& set_fingerprint(std::optional<std::string> fingerprint);

    Description Build();

private:
    Type type_ = Type::UNSPEC;
    Role role_ = Role::ACT_PASS;
    std::optional<std::string> ice_ufrag_ = std::nullopt;
    std::optional<std::string> ice_pwd_ = std::nullopt;
    std::optional<std::string> fingerprint_ = std::nullopt;

};

// Parser
class RTC_CPP_EXPORT Parser {
public:
    static Description Parse(const std::string& sdp, Type type);
};

// Description
public:
    virtual ~Description();

    Type type() const;
    Role role() const;
    const std::string bundle_id() const;
    std::optional<std::string> ice_ufrag() const;
    std::optional<std::string> ice_pwd() const;
    std::optional<std::string> fingerprint() const;

    void HintType(Type type);
    void HintRole(Role role);

    bool HasMid(const std::string_view mid) const;
    bool HasMedia() const;
    bool HasAudio() const;
    bool HasVideo() const;
    bool HasApplication() const;

    Application* SetApplication(Application app);
    const Application* application() const;
    Application* application();
    
    Media* AddMedia(Media media);
    void RemoveMedia(const std::string_view mid);
    const Media* media(const std::string_view mid) const;
    Media* media(const std::string_view mid);
    void ForEach(std::function<void(const Media&)> handler) const;

    // Clear all media and application entries.
    void ClearMediaEntries();
    
    operator std::string() const;
    std::string GenerateSDP(const std::string eol, bool application_only = false) const;

private:
    Description(Type type = Type::UNSPEC, 
                Role role = Role::ACT_PASS, 
                std::optional<std::string> ice_ufrag = std::nullopt, 
                std::optional<std::string> ice_pwd = std::nullopt, 
                std::optional<std::string> fingerprint = std::nullopt);

private:
    Type type_;
    Role role_;
    SessionEntry session_entry_; 

    std::vector<std::shared_ptr<Media>> medias_;
    std::shared_ptr<Application> application_;
    // Used for generating SDP
    std::map<std::string, std::weak_ptr<MediaEntry>> media_entries_;

};

}
}

#endif