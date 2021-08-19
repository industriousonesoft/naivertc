#include "rtc/rtp_rtcp/rtp/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

struct ExtensionInfo {
  RtpExtensionType type;
  const char* uri;
};

template <typename Extension>
constexpr ExtensionInfo CreateExtensionInfo() {
  return {Extension::kType, Extension::kUri};
}

constexpr ExtensionInfo kExtensions[] = {
    CreateExtensionInfo<AbsoluteSendTimeExtension>(),
    CreateExtensionInfo<AbsoluteCaptureTimeExtension>(),
    CreateExtensionInfo<TransmissionOffsetExtension>(),
    CreateExtensionInfo<TransportSequenceNumberExtension>(),
    CreateExtensionInfo<PlayoutDelayLimitsExtension>(),
    CreateExtensionInfo<RtpMidExtension>()
};

}  // namespace

RtpHeaderExtensionManager::RtpHeaderExtensionManager() : RtpHeaderExtensionManager(false) {}

RtpHeaderExtensionManager::RtpHeaderExtensionManager(bool extmap_allow_mixed) 
    : extmap_allow_mixed_(extmap_allow_mixed) {
    for (auto& id : extension_ids_) {
        id = kInvalidId;
    }
}   

RtpHeaderExtensionManager::~RtpHeaderExtensionManager() = default;

RtpExtensionType RtpHeaderExtensionManager::GetType(int id) const {
    if (id < kMinId || id > kMaxId) {
        return RtpExtensionType::NONE;
    }
    for (int type = int(RtpExtensionType::NONE) + 1; type < int(RtpExtensionType::NUMBER_OF_EXTENSIONS);
        ++type) {
        if (extension_ids_[type] == id) {
        return static_cast<RtpExtensionType>(type);
        }
    }
    return kInvalidType;
}

bool RtpHeaderExtensionManager::RegisterByType(int id, RtpExtensionType type) {
    for (const ExtensionInfo& extension : kExtensions)
        if (type == extension.type)
            return Register(id, extension.type, extension.uri);
    return false;
}

bool RtpHeaderExtensionManager::RegisterByUri(int id, std::string_view uri) {
    for (const ExtensionInfo& extension : kExtensions)
        if (uri == extension.uri)
            return Register(id, extension.type, extension.uri);
    PLOG_WARNING << "Unknown extension uri:'" << uri << "', id: " << id
                        << '.';
    return false;
}

void RtpHeaderExtensionManager::Deregister(RtpExtensionType type) {
    if (IsRegistered(type)) {
        extension_ids_[int(type)] = kInvalidId;
    }
}

void RtpHeaderExtensionManager::Deregister(std::string_view uri) {
    for (const ExtensionInfo& extension : kExtensions) {
        if (extension.uri == uri) {
            extension_ids_[int(extension.type)] = kInvalidId;
            break;
        }
    }
}

// Private methods
bool RtpHeaderExtensionManager::Register(int id, RtpExtensionType type, const char* uri) {
    if (type <= RtpExtensionType::NONE || type >= RtpExtensionType::NUMBER_OF_EXTENSIONS) {
        PLOG_WARNING << "Invalid RTP extension type: " << int(type);
        return false;
    }

    if (id < kMinId || id > kMaxId) {
        PLOG_WARNING << "Failed to register extension uri:'" << uri
                     << "' with invalid id:" << id << ".";
        return false;
    }

    RtpExtensionType registered_type = GetType(id);
    if (registered_type == type) {  // Same type/id pair already registered.
        PLOG_VERBOSE << "Reregistering extension uri:'" << uri
                     << "', id:" << id;
        return true;
    }

    if (registered_type != kInvalidType) {  // |id| used by another extension type.
        PLOG_WARNING << "Failed to register extension uri:'" << uri
                     << "', id:" << id
                     << ". Id already in use by extension type "
                     << static_cast<int>(registered_type);
        return false;
    }
    extension_ids_[int(type)] = static_cast<uint8_t>(id);
    return true;
}
    
} // namespace naivertc
