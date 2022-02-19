#ifndef _RTC_RTP_RTCP_RTP_HEADER_EXTENSION_MAP_H_
#define _RTC_RTP_RTCP_RTP_HEADER_EXTENSION_MAP_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <vector>
#include <string>

namespace naivertc {
namespace rtp {

// Extension size
struct ExtensionSize {
    RtpExtensionType type;
    size_t size;
};

// Header extension map
class HeaderExtensionMap {
public:
    static constexpr RtpExtensionType kInvalidType = kRtpExtensionNone;
    
public:
    HeaderExtensionMap();
    explicit HeaderExtensionMap(bool extmap_allow_mixed);
    explicit HeaderExtensionMap(ArrayView<const RtpExtension> extensions);
    ~HeaderExtensionMap();

    bool extmap_allow_mixed() const { return extmap_allow_mixed_; }
    void set_extmap_allow_mixed(bool allow_mixed) { extmap_allow_mixed_ = allow_mixed; };

    // Return kInvalidType if not found.
    RtpExtensionType GetType(int id) const;
    // Return kInvalidId if not found.
    uint8_t GetId(RtpExtensionType type) const;

    template <typename Extension>
    bool Register(int id) {
        return Register(id, Extension::kType, Extension::kUri);
    }
    bool IsRegistered(RtpExtensionType type) const;
    bool RegisterByType(RtpExtensionType type, int id);
    bool RegisterByUri(std::string_view uri, int id);
    // Return kInvalid if not registered, otherwise the registered id
    int Deregister(RtpExtensionType type);
    // Return kInvalid if not registered, otherwise the registered id
    int Deregister(std::string_view uri);

    size_t CalculateSize(ArrayView<const ExtensionSize> extensions);
    
private:
    bool Register(int id, RtpExtensionType type, std::string_view uri);

private:
    uint8_t extension_ids_[kRtpExtensionNumberOfExtensions];
    bool extmap_allow_mixed_;
};
    
} // namespace rtp
} // namespace naivertc


#endif