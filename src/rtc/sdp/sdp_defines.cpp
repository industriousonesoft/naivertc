#include "rtc/sdp/sdp_defines.hpp"

namespace naivertc {
namespace sdp {

std::ostream& operator<<(std::ostream& out, Type type) {
    switch (type) {
    case sdp::Type::UNSPEC:
        out << "unspec";
        break;
    case sdp::Type::OFFER:
        out << "offer";
        break;
    case sdp::Type::ANSWER:
        out << "answer";
        break;
    case sdp::Type::PRANSWER:
        out << "pranswer";
        break;
    case sdp::Type::ROLLBACK:
        out << "rollback";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, Role role) {
    switch (role) {
    case sdp::Role::ACT_PASS:
        out << "actpass";
        break;
    case sdp::Role::PASSIVE:
        out << "passive";
        break;
    case sdp::Role::ACTIVE:
        out << "active";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, Direction direction) {
    switch (direction)
    {
    case sdp::Direction::INACTIVE:
        out << "inactive";
        break;
    case sdp::Direction::SEND_ONLY:
        out << "sendonly";
        break;
    case sdp::Direction::RECV_ONLY:
        out << "recvonly";
        break;
    case sdp::Direction::SEND_RECV:
        out << "sendrecv";
        break;
    default:
        break;
    }
    return out;
}

} // namespace sdp
} // namespace naivertc
