#include "rtc/sdp/sdp_session_entry.hpp"
#include "common/utils.hpp"

#include <sstream>

namespace naivertc {
namespace sdp {

SessionEntry::SessionEntry() {

}

std::string SessionEntry::user_name() const {
    return user_name_;
}

std::string SessionEntry::session_id() const {
    return session_id_;
}

std::string SessionEntry::GenerateSDP(std::string_view eol, Role role) const {
    std::ostringstream oss;
    std::string sp = " ";

    // sdp版本号，一直为0,rfc4566规定
    oss << "v=0" << eol;
    // 会话发起者
    // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
    // username如何没有使用-代替，7017624586836067756是整个会话的编号，2代表会话版本，如果在会话
    // 过程中有改变编码之类的操作，重新生成sdp时,sess-id不变，sess-version加1
    // eg: o=- 7017624586836067756 2 IN IP4 127.0.0.1
    oss << "o=" << user_name_ << sp << session_id_ << " 0 IN IP4 127.0.0.1" << eol;
    // 会话名，没有的话使用-代替
    oss << "s=-" << eol;
    // 两个值分别是会话的起始时间和结束时间，这里都是0代表没有限制
    oss << "t=0 0" << eol;

    return oss.str();
} 

void SessionEntry::ParseSDPLine(std::string_view line) {

    // version
    if (utils::string::match_prefix(line, "v=")) {
        // Ignore
    }
    // session initiator
    else if (utils::string::match_prefix(line, "o=")) {
        std::string value{line.substr(2)};
        std::istringstream origin(std::move(value));
        origin >> user_name_ >> session_id_;
    }
    // session name
    else if (utils::string::match_prefix(line, "s=")) {
        // Ignore
    }
    // session time range
    else if (utils::string::match_prefix(line, "t=")) {
        // Ignore
    }
    // attribute line
    else if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);

        // a=group:BUNDLE
        if (key == "group") {

        // a=msid-semantic: WMS
        }else if (key == "msid-semantic") {

        }else {

        }

    }else {

    }

    // username如果没有则使用'-'代替
    if (user_name_.empty()) {
        user_name_ = "-";
    }

    if (session_id_.empty()) {
        session_id_ = std::to_string(utils::random::generate_random<uint32_t>());
    }
}

} // namespace sdp
} // namespace naivert 