#ifndef _RTC_SDP_SESSION_DESCRIPTION_H_
#define _RTC_SDP_SESSION_DESCRIPTION_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"
#include "rtc/sdp/sdp_session_entry.hpp"
#include "rtc/sdp/sdp_media_entry_application.hpp"
#include "rtc/sdp/sdp_media_entry_audio.hpp"
#include "rtc/sdp/sdp_media_entry_video.hpp"

#include <string>
#include <variant>

namespace naivertc {
namespace sdp {

class RTC_CPP_EXPORT SessionDescription {
public:
    SessionDescription(const std::string& sdp, Type type = Type::UNSPEC, Role role = Role::ACT_PASS);
    SessionDescription(const std::string& sdp, std::string type_string);

    Type type() const;
    Role role() const;
    std::string bundle_id() const;
    std::optional<std::string> ice_ufrag() const;
    std::optional<std::string> ice_pwd() const;
    std::optional<std::string> fingerprint() const;

    void hintType(Type type);
    void set_fingerprint(std::string fingerprint);

    operator std::string() const;
    std::string GenerateSDP(std::string_view eol, bool application_only = false) const;

    bool HasApplication() const;
    bool HasAudio() const;
    bool HasVieo() const;
    bool HasMid(std::string_view mid) const;

    int AddMedia(Media media);
    int AddApplication(Application app);
    int AddApplication(std::string mid = "data");
    int AddAudio(std::string mid = "audio", Direction direction = Direction::SEND_ONLY);
    int AddVideo(std::string mid = "video", Direction direction = Direction::SEND_ONLY);
    void ClearMedia();

    std::variant<Media*, Application*> media(unsigned int index);
    std::variant<const Media*, const Application*> media(unsigned int index) const;
    unsigned int media_count() const;

    const Application* application() const;
    Application* application();

private:
    std::shared_ptr<MediaEntry> CreateMediaEntry(std::string mline, std::string mid, Direction direction);

    void Parse(std::string sdp);

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