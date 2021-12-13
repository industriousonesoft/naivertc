#include "rtc/congestion_controller/goog_cc/defines.hpp"

namespace naivertc {

std::ostream& operator<<(std::ostream& out, BandwidthUsage usage) {
    switch (usage) {
    case BandwidthUsage::NORMAL:
        out << "NORMAL";
        break;
    case BandwidthUsage::UNDERUSING:
        out << "UNDERUSING";
        break;
    case BandwidthUsage::OVERUSING:
        out << "OVERUSING";
        break;
    default:
        break;
    }
    return out;
}

} // namespace naivertc