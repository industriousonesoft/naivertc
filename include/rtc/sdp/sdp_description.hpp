#ifndef _RTC_SDP_DESCRIPTION_H_
#define _RTC_SDP_DESCRIPTION_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"
#include "rtc/sdp/sdp_session_entry.hpp"
#include "rtc/sdp/sdp_media_entry_application.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"

#include <string>
#include <variant>

namespace naivertc {
namespace sdp {

class RTC_CPP_EXPORT Description {
public:
// Builder
class RTC_CPP_EXPORT Builder {
public:
    Builder();
    ~Builder();

    Builder& set_type(Type type);
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

public:
    Description(const std::string& sdp, Type type = Type::UNSPEC, Role role = Role::ACT_PASS);
    Description(const std::string& sdp, const std::string& type_string);

    virtual ~Description();

    Type type() const;
    Role role() const;
    const std::string bundle_id() const;
    std::optional<std::string> ice_ufrag() const;
    std::optional<std::string> ice_pwd() const;
    std::optional<std::string> fingerprint() const;

    void hintType(Type type);
    void ClearMedia();

    operator std::string() const;
    std::string GenerateSDP(std::string_view eol, bool application_only = false) const;

    bool HasApplication() const;
    bool HasAudio() const;
    bool HasVieo() const;
    bool HasMid(std::string_view mid) const;

    std::variant<Media*, Application*> media(unsigned int index);
    std::variant<const Media*, const Application*> media(unsigned int index) const;
    unsigned int media_count() const;

    const Application* application() const;
    Application* application();

    void AddApplication(Application app);
    void AddApplication(std::string mid = "data");
    void AddMedia(Media media);
    void AddAudio(std::string mid = "audio", Direction direction = Direction::SEND_ONLY);
    void AddVideo(std::string mid = "video", Direction direction = Direction::SEND_ONLY);

protected:
    Description(Type type = Type::UNSPEC, 
                Role role = Role::ACT_PASS, 
                std::optional<std::string> ice_ufrag = std::nullopt, 
                std::optional<std::string> ice_pwd = std::nullopt, 
                std::optional<std::string> fingerprint = std::nullopt);

private:
    std::shared_ptr<MediaEntry> CreateMediaEntry(const std::string& mline, const std::string mid, Direction direction);

    void Parse(const std::string& sdp);

protected:
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