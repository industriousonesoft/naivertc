#ifndef _PC_SDP_SESSION_DESCRIPTION_H_
#define _PC_SDP_SESSION_DESCRIPTION_H_

#include "base/defines.hpp"
#include "pc/sdp/sdp_entry.hpp"
#include "pc/sdp/sdp_defines.hpp"

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

    void hintType(Type type);
    void set_fingerprint(std::string fingerprint);

    std::string GenerateSDP(std::string_view eol, bool application_only = false) const;

    bool HasApplication() const;
    bool HasAudio() const;
    bool HasVieo() const;
    bool HasMid() const;

    int AddMedia(Media media);
    int AddApplication(Application app);
    int AddApplication(std::string mid = "data");
    int AddAudio(std::string mid = "audio", Direction direction = Direction::SEND_ONLY);
    int AddVideo(std::string mid = "video", Direction direction = Direction::SEND_ONLY);
    void ClearMedia();

    std::variant<Media*, Application*> media(unsigned int index);
    std::variant<const Media*, const Application*> media(unsigned int index) const;
    unsigned int media_count() const;

private:
    std::shared_ptr<Entry> CreateEntry(std::string mline, std::string mid, Direction direction);

    static bool IsSHA256Fingerprint(std::string_view fingerprint);

private:

    Type type_;
    // Session-level attributes
    Role role_;
    std::string user_name_;
    std::string session_id_;
    std::optional<std::string> ice_ufrag_;
    std::optional<std::string> ice_pwd_;
    std::optional<std::string> fingerprint_;

    // Entries
    std::vector<std::shared_ptr<Entry>> entries_;

};

}
}

#endif