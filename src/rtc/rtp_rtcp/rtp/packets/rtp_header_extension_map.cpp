#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_map.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtp {
namespace {

struct ExtensionInfo {
  RtpExtensionType type;
  std::string_view uri;
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
    CreateExtensionInfo<TransportSequenceNumberV2>(),
    CreateExtensionInfo<PlayoutDelayLimits>(),
    CreateExtensionInfo<RtpMid>(),
    CreateExtensionInfo<RtpStreamId>(),
    CreateExtensionInfo<RepairedRtpStreamId>()
};

}  // namespace

HeaderExtensionMap::HeaderExtensionMap() : HeaderExtensionMap(false) {}

HeaderExtensionMap::HeaderExtensionMap(bool extmap_allow_mixed) 
    : extmap_allow_mixed_(extmap_allow_mixed) {
    for (auto& id : extension_ids_) {
        id = RtpExtension::kInvalidId;
    }
}

HeaderExtensionMap::HeaderExtensionMap(ArrayView<const RtpExtension> extensions) 
    : HeaderExtensionMap(false) {
    for (const auto& extension : extensions) {
        RegisterByUri(extension.uri, extension.id);
    }
}

HeaderExtensionMap::~HeaderExtensionMap() = default;

RtpExtensionType HeaderExtensionMap::GetType(int id) const {
    if (id < RtpExtension::kMinId || id > RtpExtension::kMaxId) {
        return kRtpExtensionNone;
    }
    for (int type = int(kRtpExtensionNone) + 1; type < int(kRtpExtensionNumberOfExtensions);
        ++type) {
        if (extension_ids_[type] == id) {
            return static_cast<RtpExtensionType>(type);
        }
    }
    return kInvalidType;
}

uint8_t HeaderExtensionMap::GetId(RtpExtensionType type) const {
    if (type <= kRtpExtensionNone || type >= kRtpExtensionNumberOfExtensions) {
        return RtpExtension::kInvalidId;
    }
    return extension_ids_[int(type)];
}

bool HeaderExtensionMap::IsRegistered(RtpExtensionType type) const {
    return GetId(type) != RtpExtension::kInvalidId;
}

bool HeaderExtensionMap::RegisterByType(RtpExtensionType type, int id) {
    for (const ExtensionInfo& extension : kExtensions) {
        if (type == extension.type) {
            return Register(id, extension.type, extension.uri);
        }
    }
    return false;
}

bool HeaderExtensionMap::RegisterByUri(std::string_view uri, int id) {
    for (const ExtensionInfo& extension : kExtensions) {
        if (uri == extension.uri) {
            return Register(id, extension.type, extension.uri);
        }
    }  
    PLOG_WARNING << "Unknown extension uri='" << uri 
                 << "', id=" << id << '.';
    return false;
}

int HeaderExtensionMap::Deregister(RtpExtensionType type) {
    int registered_id = RtpExtension::kInvalidId;
    if (IsRegistered(type)) {
        registered_id = extension_ids_[int(type)];
        extension_ids_[int(type)] = RtpExtension::kInvalidId;
    }
    return registered_id;
}

int HeaderExtensionMap::Deregister(std::string_view uri) {
    int registered_id = RtpExtension::kInvalidId;
    for (const ExtensionInfo& extension : kExtensions) {
        if (extension.uri == uri) {
            registered_id = extension_ids_[int(extension.type)];
            extension_ids_[int(extension.type)] = RtpExtension::kInvalidId;
            break;
        }
    }
    return registered_id;
}

// Private methods
bool HeaderExtensionMap::Register(int id, RtpExtensionType type, std::string_view uri) {
    if (type <= kRtpExtensionNone || type >= kRtpExtensionNumberOfExtensions) {
        PLOG_WARNING << "Invalid RTP extension type: " << int(type);
        return false;
    }

    if (id < RtpExtension::kMinId || id > RtpExtension::kMaxId) {
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
size_t HeaderExtensionMap::CalculateSize(ArrayView<const ExtensionSize> extensions) {
    // RFC3350 Section 5.3.1
    static constexpr size_t kExtensionBlockHeaderSize = 4;

    size_t extension_size = 0;
    size_t num_extensions = 0;
    size_t each_extension_header_size = 1;
    for (const auto& extension : extensions) {
        // Filter the unregistered extensions.
        size_t id = GetId(extension.type);
        if (id != RtpExtension::kInvalidId) {
            continue;
        }
        // All extensions should use same size header. Check if the |extension|
        // forces to switch to two byte header that allows larger id and value size.
        if (id > RtpExtension::kOneByteHeaderExtensionMaxId ||
            extension.size > RtpExtension::kOneByteHeaderExtensionMaxValueSize) {
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
    
} // namespace rtp
} // namespace naivertc
