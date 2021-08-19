#ifndef _RTC_RTP_RTCP_RTP_HEADER_EXTENSION_MAP_H_
#define _RTC_RTP_RTCP_RTP_HEADER_EXTENSION_MAP_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <vector>
#include <string>

namespace naivertc {

class RTC_CPP_EXPORT RtpHeaderExtensionManager {
public:
    static constexpr RtpExtensionType kInvalidType = RtpExtensionType::NONE;
    static constexpr int kInvalidId = 0;
    static constexpr int kMinId = 1;
    static constexpr int kMaxId = 255;

public:
    RtpHeaderExtensionManager();
    explicit RtpHeaderExtensionManager(bool extmap_allow_mixed);
    ~RtpHeaderExtensionManager();

    bool extmap_allow_mixed() const { return extmap_allow_mixed_; }
    void set_extmap_allow_mixed(bool allow_mixed) { extmap_allow_mixed_ = allow_mixed; };

    // Return kInvalidType if not found.
    RtpExtensionType GetType(int id) const;
    // Return kInvalidId if not found.
    uint8_t GetId(RtpExtensionType type) const {
        if (type <= RtpExtensionType::NONE || type >= RtpExtensionType::NUMBER_OF_EXTENSIONS) {
            return kInvalidId;
        }
        return extension_ids_[int(type)];
    }

    template <typename Extension>
    bool Register(int id) {
        return Register(id, Extension::kType, Extension::kUri);
    }
    bool RegisterByType(int id, RtpExtensionType type);
    bool RegisterByUri(int id, std::string_view uri);

    bool IsRegistered(RtpExtensionType type) const {
        return GetId(type) != kInvalidId;
    }

    void Deregister(RtpExtensionType type);
    void Deregister(std::string_view uri);

private:
    bool Register(int id, RtpExtensionType type, const char* uri);

private:
    uint8_t extension_ids_[int(RtpExtensionType::NUMBER_OF_EXTENSIONS)];
    bool extmap_allow_mixed_;
};
    
} // namespace naivertc


#endif