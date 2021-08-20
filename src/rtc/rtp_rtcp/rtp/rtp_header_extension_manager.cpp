#include "rtc/rtp_rtcp/rtp/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_header_extensions.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtp {
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
    CreateExtensionInfo<AbsoluteSendTime>(),
    CreateExtensionInfo<AbsoluteCaptureTime>(),
    CreateExtensionInfo<TransmissionTimeOffset>(),
    CreateExtensionInfo<TransportSequenceNumber>(),
    CreateExtensionInfo<PlayoutDelayLimits>(),
    CreateExtensionInfo<RtpMid>()
};

}  // namespace

ExtensionManager::ExtensionManager() : ExtensionManager(false) {}

ExtensionManager::ExtensionManager(bool extmap_allow_mixed) 
    : extmap_allow_mixed_(extmap_allow_mixed) {
    for (auto& id : extension_ids_) {
        id = kInvalidId;
    }
}   

ExtensionManager::~ExtensionManager() = default;

RtpExtensionType ExtensionManager::GetType(int id) const {
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

bool ExtensionManager::RegisterByType(int id, RtpExtensionType type) {
    for (const ExtensionInfo& extension : kExtensions)
        if (type == extension.type)
            return Register(id, extension.type, extension.uri);
    return false;
}

bool ExtensionManager::RegisterByUri(int id, std::string_view uri) {
    for (const ExtensionInfo& extension : kExtensions)
        if (uri == extension.uri)
            return Register(id, extension.type, extension.uri);
    PLOG_WARNING << "Unknown extension uri:'" << uri << "', id: " << id
                        << '.';
    return false;
}

int ExtensionManager::Deregister(RtpExtensionType type) {
    int registered_id = kInvalidId;
    if (IsRegistered(type)) {
        registered_id = extension_ids_[int(type)];
        extension_ids_[int(type)] = kInvalidId;
    }
    return registered_id;
}

int ExtensionManager::Deregister(std::string_view uri) {
    int registered_id = kInvalidId;
    for (const ExtensionInfo& extension : kExtensions) {
        if (extension.uri == uri) {
            registered_id = extension_ids_[int(extension.type)];
            extension_ids_[int(extension.type)] = kInvalidId;
            break;
        }
    }
    return registered_id;
}

std::optional<HeaderExtension> ExtensionManager::ParseExtension(int id, const uint8_t* data, size_t size) {
    // Check the current extension is registered or not
    RtpExtensionType extension_type = GetType(id);
    // Only parse the supported extensions
    if (extension_type == kInvalidType) {
         PLOG_WARNING << "Unsupported extension with id: " << id;
         return std::nullopt;
    }
    std::optional<HeaderExtension> extension = std::nullopt;
    switch (extension_type) {
    case RtpExtensionType::ABSOLUTE_SEND_TIME: 
        extension = std::optional<AbsoluteSendTime>();
        break;
    case RtpExtensionType::ABSOLUTE_CAPTURE_TIME: 
        extension = std::optional<AbsoluteCaptureTime>();
        break;
    case RtpExtensionType::TRANSPORT_SEQUENCE_NUMBER:
        extension = std::optional<TransportSequenceNumber>();
        break;
    case RtpExtensionType::TRANSMISSTION_TIME_OFFSET:
        extension = std::optional<TransmissionTimeOffset>();
        break;
    case RtpExtensionType::PLAYOUT_DELAY_LIMITS:
        extension = std::optional<PlayoutDelayLimits>();
    case RtpExtensionType::MID:
        extension = std::optional<RtpMid>();
    default:
        break;
    }

    return extension->Parse(data, size) ? extension : std::nullopt;
}

// Private methods
bool ExtensionManager::Register(int id, RtpExtensionType type, const char* uri) {
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
    
} // namespace rtc
} // namespace naivertc
