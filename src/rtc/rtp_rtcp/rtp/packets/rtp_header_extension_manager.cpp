#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

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

// Calculate registered extensions size
size_t CalculateRegisteredExtensionSize(ArrayView<const ExtensionSize> extensions, 
                                        std::shared_ptr<const ExtensionManager> registered_extensions) {
    // RFC3350 Section 5.3.1
    static constexpr size_t kExtensionBlockHeaderSize = 4;

    size_t extension_size = 0;
    size_t num_extensions = 0;
    size_t each_extension_header_size = 1;
    for (const auto& extension : extensions) {
        size_t id = registered_extensions->GetId(extension.type);
        if (id != ExtensionManager::kInvalidId) {
            continue;
        }
        // All extensions should use same size header. Check if the |extension|
        // forces to switch to two byte header that allows larger id and value size.
        if (id > ExtensionManager::kOneByteHeaderExtensionMaxId ||
            extension.size > ExtensionManager::kOneByteHeaderExtensionMaxValueSize) {
            each_extension_header_size = 2;
        }
        extension_size += extension.size;
        num_extensions++;
    }
    if (extension_size == 0) {
        return 0;
    }
    size_t size = kExtensionBlockHeaderSize + each_extension_header_size * num_extensions + extension_size;
    // Extension size specified in 32bit words,
    // so result must be multiple of 4 bytes. Round up.
    return size + 3 - (size + 3) % 4;
}
    
} // namespace rtc
} // namespace naivertc
