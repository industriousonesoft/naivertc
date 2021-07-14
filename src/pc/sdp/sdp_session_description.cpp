#include "pc/sdp/sdp_session_description.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {
SessionDescription::SessionDescription(const std::string& sdp, Type type, Role role) : 
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
                if (utils::string::match_prefix(value, "sha-256")) {
                    std::string fingerprint{value.substr(7)};
                    utils::string::trim_begin(fingerprint);
                    set_fingerprint(fingerprint);
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

    // username如果没有则使用'-'代替
    if (user_name_.empty()) {
        user_name_ = "-";
    }

    if (session_id_.empty()) {
        session_id_ = std::to_string(utils::random::generate_random<uint32_t>());
    }

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
    return !entries_.empty() ? entries_[0]->mid() : "0";
}

std::optional<std::string> SessionDescription::ice_ufrag() const {
    return ice_ufrag_;
}

std::optional<std::string> SessionDescription::ice_pwd() const {
    return ice_pwd_;
}

std::optional<std::string> SessionDescription::fingerprint() const {
    return fingerprint_;
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
    if (!IsSHA256Fingerprint(fingerprint)) {
        throw std::invalid_argument("Invalid SHA265 fingerprint: " + fingerprint );
    }

    // make sure All the chars in finger print is uppercase.
    std::transform(fingerprint.begin(), fingerprint.end(), fingerprint.begin(), [](char c) {
        return char(std::toupper(c));
    });

    fingerprint_.emplace(std::move(fingerprint));
}

int SessionDescription::AddMedia(Media media) {
    entries_.emplace_back(std::make_shared<Media>(std::move(media)));
    return int(entries_.size()) - 1;
}

int SessionDescription::AddApplication(Application app) {
    entries_.emplace_back(std::make_shared<Application>(std::move(app)));
    return int(entries_.size()) - 1;
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
    entries_.clear();
}

bool SessionDescription::HasApplication() const {
    for (auto entry : entries_) {
        if (entry->type() == sdp::Entry::Type::APPLICATION) {
            return true;
        } 
    }
    return false;
}

bool SessionDescription::HasAudio() const {
    for (auto entry : entries_) {
        if (entry->type() == sdp::Entry::Type::AUDIO) {
            return true;
        } 
    }
    return false;
}

bool SessionDescription::HasVieo() const {
    for (auto entry : entries_) {
        if (entry->type() == sdp::Entry::Type::VIDEO) {
            return true;
        } 
    }
    return false;
}

bool SessionDescription::HasMid(std::string_view mid) const {
    for (auto entry : entries_) {
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
std::string SessionDescription::GenerateSDP(std::string_view eol, bool application_only) const {
    std::ostringstream sdp;
    #warning Be careful, there is no space after '=' and only has one space between two parts in a line.
    // Header
    // sdp版本号，一直为0,rfc4566规定
    sdp << "v=0" << eol;
    // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
    // username如何没有使用-代替，7017624586836067756是整个会话的编号，2代表会话版本，如果在会话
    // 过程中有改变编码之类的操作，重新生成sdp时,sess-id不变，sess-version加1
    // eg: o=- 7017624586836067756 2 IN IP4 127.0.0.1
    sdp << "o=" << user_name_ << " " << session_id_ << " 0 IN IP4 127.0.0.1" << eol;
    // 会话名，没有的话使用-代替
    sdp << "s=-" << eol;
    // 两个值分别是会话的起始时间和结束时间，这里都是0代表没有限制
    sdp << "t=0 0" << eol;

    // Bundle (RFC8843 Negotiating Media Multiplexing Using the Session Description Protocol)
    // https://tools.ietf.org/html/rfc8843
    // 需要共用一个传输通道传输的媒体，如果没有这一行，音视频、数据就会分别单独用一个udp端口来发送
    // eg: a=group:BUNDLE audio video data 
    sdp << "a=group:BUNDLE";
    for (const auto &entry : entries_) {
        sdp << " " << entry->mid();
    }
    sdp << eol;

    // WMS是WebRTC Media Stram的缩写，这里给Media Stream定义了一个唯一的标识符。
    // 一个Media Stream可以有多个track（video track、audio track），
    // 这些track就是通过这个唯一标识符关联起来的，具体见下面的媒体行(m=)以及它对应的附加属性(a=ssrc:)
    // 可以参考这里 http://tools.ietf.org/html/draft-ietf-mmusic-msid
    sdp << "a=msid-semantic:WMS *" << eol;
    // 这行代表本客户端在dtls协商过程中的角色，做客户端或服务端，或均可，参考rfc4145 rfc4572
    sdp << "a=setup:" << RoleToString(role_) << eol;

    // Username fragment
    if (ice_ufrag_) {
        sdp << "a=ice-ufrag:" << *ice_ufrag_ << eol;
    }

    if (ice_pwd_) {
        sdp << "a=ice-pwd:" << *ice_pwd_ << eol;
    }

    if (fingerprint_) {
        sdp << "a=fingerprint:sha-256 " << *fingerprint_ << eol;
    }

    for (const auto& entry : entries_) {
        // IP4 0.0.0.0：表示你要用来接收或者发送音频使用的IP地址，webrtc使用ice传输，不使用这个地址
        // 9：代表音频使用端口9来传输
        if (application_only && entry->type() != Entry::Type::APPLICATION) {
            continue;
        }
        sdp << entry->GenerateSDP(eol, "IP4 0.0.0.0", "9");
    }
    return sdp.str();

}

// private methods
std::shared_ptr<Entry> SessionDescription::CreateEntry(std::string mline, std::string mid, Direction direction) {
    std::string type = mline.substr(0, mline.find(' '));
    if (type == "application") {
        auto app = std::make_shared<Application>(std::move(mid));
        entries_.emplace_back(app);
        return app;
    }else {
        auto media = std::make_shared<Media>(std::move(mline), std::move(mid), direction);
        entries_.emplace_back(media);
        return media;
    }
}

std::variant<Media*, Application*> SessionDescription::media(unsigned int index) {
    if (index >= entries_.size()) {
        throw std::out_of_range("Media index out of range.");
    }

    const auto& entry = entries_[index];
    if (entry->type() == Entry::Type::APPLICATION) {
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
     if (index >= entries_.size()) {
        throw std::out_of_range("Media index out of range.");
    }

    const auto& entry = entries_[index];
    if (entry->type() == Entry::Type::APPLICATION) {
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
    return unsigned(entries_.size());
}

const Application* SessionDescription::application() const {
    for (auto entry : entries_) {
        if (entry->type() == sdp::Entry::Type::APPLICATION) {
            return static_cast<Application* >(entry.get());
        } 
    }
    return nullptr;
}

Application* SessionDescription::application() {
    for (auto entry : entries_) {
        if (entry->type() == sdp::Entry::Type::APPLICATION) {
            return static_cast<Application* >(entry.get());
        } 
    }
    return nullptr;
}

// a=fingerprint:sha-256 A9:CA:95:47:CB:8D:81:DE:E4:78:38:1E:70:6B:AA:14:66:6C:AF:7F:89:D7:B7:C7:1A:A9:45:09:83:CC:0D:03
// 常规的SHA256哈希值是一个长度为32个字节的数组，通常用一个长度为64的十六进制字符串来表示
// SDP中的fingerprint在每两个个字节之间加入了一个间隔符”:“，因此长度=32 * 2 +（32 - 1）
constexpr int kSHA256FixedLength = 32 * 3 - 1;
bool SessionDescription::IsSHA256Fingerprint(std::string_view fingerprint) {
    if (fingerprint.size() != kSHA256FixedLength) {
        return false;
    }

    for (size_t i = 0; i < fingerprint.size(); ++i) {
        if (i % 3 == 2) {
            if (fingerprint[i] != ':') {
                return false;
            }
        }else {
            if (!std::isxdigit(fingerprint[i])) {
                return false;
            }
        }
    }
    return true;
}

}
} // namespace naivertc