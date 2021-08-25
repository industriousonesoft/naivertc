#ifndef _RTC_RTP_RTCP_RTP_HEADER_EXTENSION_MAP_H_
#define _RTC_RTP_RTCP_RTP_HEADER_EXTENSION_MAP_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <vector>
#include <string>

namespace naivertc {
namespace rtp {

class RTC_CPP_EXPORT ExtensionManager {
public:
    static constexpr RtpExtensionType kInvalidType = RtpExtensionType::NONE;
    static constexpr int kInvalidId = 0;
    static constexpr int kMinId = 1;
    static constexpr int kMaxId = 255;
    static constexpr int kMaxValueSize = 255;
    static constexpr int kOneByteHeaderExtensionMaxId = 14;
    static constexpr int kOneByteHeaderExtensionMaxValueSize = 16;

public:
    ExtensionManager();
    explicit ExtensionManager(bool extmap_allow_mixed);
    ~ExtensionManager();

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

    // Return kInvalid if not registered, otherwise the registered id
    int Deregister(RtpExtensionType type);
    // Return kInvalid if not registered, otherwise the registered id
    int Deregister(std::string_view uri);
    
private:
    bool Register(int id, RtpExtensionType type, const char* uri);

private:
    uint8_t extension_ids_[int(RtpExtensionType::NUMBER_OF_EXTENSIONS)];
    bool extmap_allow_mixed_;
};
    
} // namespace rtc
} // namespace naivertc


#endif