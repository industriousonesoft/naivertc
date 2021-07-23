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
#include <variant>
#include <vector>

namespace naivertc {
namespace sdp {

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

    operator std::string() const;
    std::string GenerateSDP(const std::string eol, bool application_only = false) const;

    bool HasApplication() const;
    bool HasAudio() const;
    bool HasVideo() const;
    bool HasMid(std::string_view mid) const;

    std::variant<std::shared_ptr<Media>, std::shared_ptr<Application>> media(unsigned int index) const;
    
    unsigned int media_count() const;

    std::shared_ptr<Application> application() const;
    std::shared_ptr<Media> media(std::string_view mid) const;

    void AddApplication(std::shared_ptr<Application> app);
    void AddApplication(Application app);
    void AddMedia(Media media);
    void AddMedia(std::shared_ptr<Media> media);

    std::shared_ptr<Application> AddApplication(std::string mid = "data");
    std::shared_ptr<Audio> AddAudio(std::string mid = "audio", Direction direction = Direction::SEND_ONLY);
    std::shared_ptr<Video> AddVideo(std::string mid = "video", Direction direction = Direction::SEND_ONLY);

    void ClearMedia();

private:
    Description(Type type = Type::UNSPEC, 
                Role role = Role::ACT_PASS, 
                std::optional<std::string> ice_ufrag = std::nullopt, 
                std::optional<std::string> ice_pwd = std::nullopt, 
                std::optional<std::string> fingerprint = std::nullopt);

private:
    Type type_;
    Role role_;
    // Session-level entries
    SessionEntry session_entry_; 
    // Media-level Entries
    std::vector<std::shared_ptr<MediaEntry>> media_entries_;

};

}
}

#endif